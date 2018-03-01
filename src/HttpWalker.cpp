#include "HttpWalker.hpp"
#include <array>
#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <regex>
#include <thread>
#include <unordered_set>

using std::move;

struct job {
	boost::asio::ip::tcp::socket socket;
	std::string host;
	std::string path;
	std::function<void(const std::string &url, unsigned short status,
	                   std::vector<std::string> links)>
	    complete;
	boost::asio::streambuf write_buffer;
	boost::asio::streambuf read_buffer;
	unsigned short status = -1;

	job(boost::asio::io_service &ctx_, std::string host_, std::string path_,
	    std::function<void(const std::string &url, unsigned short status,
	                       std::vector<std::string> links)>
	        complete_)
	    : socket{ctx_}, host{host_}, path{move(path_)}, complete{move(complete_)} {
	}
};

class Walk {
	boost::asio::io_service ctx_;

	HttpWalker *emitter_;
	std::string host_;
	boost::asio::ip::tcp::resolver::endpoint_type endpoint_;
	std::mutex enqueued_mutex_;
	std::unordered_set<std::string> enqueued_;
	std::mutex visited_mutex_;
	std::unordered_set<std::string> visited_;
	std::future<void> stop_;

	std::future<void> task_;
	std::promise<void> to_stop_;
	void enqueue(const std::string &url);

public:
	Walk() = delete;
	Walk(const Walk &) = delete;
	Walk(Walk &&second) = delete;
	Walk &operator=(const Walk &) = delete;
	Walk &operator=(Walk &&) = delete;
	Walk(HttpWalker *emitter, std::string url);
	~Walk() = default;
};

HttpWalker::HttpWalker() {}
HttpWalker::~HttpWalker() {}

void HttpWalker::stop() { current_walk = {}; }
void HttpWalker::start(const std::string &url) {
	current_walk = std::make_unique<Walk>(this, url);
}
Walk::Walk(HttpWalker *emitter, std::string url) : emitter_{emitter}, ctx_{} {
	// std::cout << "WALK CONSTRUCTOR" << std::endl;
	host_ = url.substr(0, url.find('/'));
	std::string path = url.substr(url.find('/'));
	stop_ = to_stop_.get_future();
	task_ = std::async(std::launch::async, [ this, path = move(path) ] {
		{
			boost::asio::ip::tcp::resolver resolver{ctx_};
			auto endpoints = resolver.resolve({host_, "80"});
			boost::asio::ip::tcp::socket tester{ctx_};
			endpoint_ = connect(tester, endpoints);
			std::cout << endpoint_ << std::endl;
		}
		enqueue(path);
		std::vector<std::future<void>> cotasks(
		    std::max(2u, std::thread::hardware_concurrency()) - 1);
		for (auto &t : cotasks)
			t = std::async(std::launch::async, [this] { ctx_.run(); });
	});
}

static void
connect_job(std::unique_ptr<job> job_ptr,
            boost::asio::ip::tcp::resolver::endpoint_type endpoints);
static void send_job(std::unique_ptr<job> job_ptr);
static void read_status_job(std::unique_ptr<job> job_ptr);
static void recieve_job(std::unique_ptr<job> job_ptr);

std::string collapse(std::string s) {
	s = s.substr(0, s.find('#'));
	while (true) {
		size_t pos = s.find("/../");
		if (pos == std::string::npos)
			break;
		if (pos == 0)
			break;
		size_t start = s.rfind('/', pos - 1);
		if (start == std::string::npos)
			break;
		s.replace(start, pos + 3 - start, "");
	}
	return s;
}
void Walk::enqueue(const std::string &raw_path) {
	std::string path = collapse(raw_path);
	{
		std::lock_guard lk{enqueued_mutex_};
		if (enqueued_.find(path) != enqueued_.end())
			return;
		using std::chrono::operator""s;
		if (stop_.wait_for(0s) != std::future_status::timeout) {
			// std::cout << "TIMEOUT " << path << std::endl;
			return;
		}
		// std::cout << "ENQUEUE " << path << std::endl;
		enqueued_.insert(path);
		emit emitter_->progress(visited_.size(), enqueued_.size());
	}
	auto job_ptr = std::make_unique<job>(
	    ctx_, host_, path,
	    [this](const std::string &path, unsigned short status,
	           std::vector<std::string> links) {
		    {
			    std::lock_guard lk{visited_mutex_};
			    visited_.insert(path);
			    // std::cout << "FOUND " << path <<
			    // std::endl;
		    }
		    emit emitter_->foundItem(QString(path.c_str()), status);
		    for (auto &link : links)
			    enqueue(link);
	    });
	connect_job(move(job_ptr), endpoint_);
}

static void
connect_job(std::unique_ptr<job> job_ptr,
            boost::asio::ip::tcp::resolver::endpoint_type endpoint) {
	// std::cout << "CONNECT " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	raw_ptr->socket.async_connect(
	    endpoint, [job_ptr = move(job_ptr)](boost::system::error_code ec) mutable {
		    if (!ec) {
			    send_job(move(job_ptr));
		    }
	    });
}
static void send_job(std::unique_ptr<job> job_ptr) {
	// std::cout << "SEND " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	std::ostream os{&raw_ptr->write_buffer};
	os << "GET " << raw_ptr->path << " HTTP/1.0\r\n"
	   << "Host: " << raw_ptr->host << "\r\n"
	   << "Accept: */*\r\n"
	   << "Connection: close\r\n\r\n";
	boost::asio::async_write(
	    raw_ptr->socket, raw_ptr->write_buffer,
	    [job_ptr = move(job_ptr)](boost::system::error_code ec,
	                              std::size_t bytes_transferred) mutable {
		    if (!ec) {
			    read_status_job(move(job_ptr));
		    }
	    });
}
static void read_status_job(std::unique_ptr<job> job_ptr) {
	// std::cout << "READ_STATUS " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	boost::asio::async_read_until(
	    raw_ptr->socket, raw_ptr->read_buffer, "\r\n",
	    [job_ptr = move(job_ptr)](boost::system::error_code ec,
	                              std::size_t lem) mutable {
		    if (!ec) {
			    std::string version, code_msg;
			    unsigned short code;
			    std::istream is{&job_ptr->read_buffer};
			    is >> version >> code;
			    std::getline(is, code_msg);
			    if (is && version.substr(0, 5) == "HTTP/") {
				    job_ptr->status = code;
				    return recieve_job(move(job_ptr));
			    }
		    }
		    job_ptr->complete(job_ptr->path, 0, {});
		    std::cerr << "Error: " << ec << std::endl;
	    });
}
static void recieve_job(std::unique_ptr<job> job_ptr) {
	// std::cout << "READ_CONTENT " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	boost::asio::async_read(
	    raw_ptr->socket, raw_ptr->read_buffer,
	    [job_ptr = move(job_ptr)](boost::system::error_code ec,
	                              std::size_t lem) mutable {
		    if (!ec) {
			    recieve_job(move(job_ptr));
		    } else if (ec == boost::asio::error::eof) {
			    std::vector<std::string> links;
			    std::string buf(std::istreambuf_iterator<char>{&job_ptr->read_buffer},
			                    {});
			    std::regex ahref{"<\\s*a.*href\\s*=\\s*(\"[^\"]*\"|\'[^\']*\'|\\S*)"};
			    auto begin = std::sregex_iterator(buf.begin(), buf.end(), ahref);
			    auto end = std::sregex_iterator{};
			    for (auto it = begin; it != end; it++) {
				    std::string link = (*it)[1];
				    if (link[0] == '\'' || link[0] == '"')
					    link = link.substr(1, link.size() - 2);
				    std::regex external{"\\w*:.*"};
				    if (!std::regex_match(link, external)) {
					    if (link[0] != '/')
						    link = job_ptr->path.substr(0, job_ptr->path.rfind('/') + 1) + link;
					    size_t param_pos = link.find('?');
					    if (param_pos != std::string::npos) {
						    size_t pos = param_pos;
						    while (true) {
							    pos = link.find("&amp;", pos + 1);
							    if (pos == std::string::npos)
								    break;
							    link.replace(pos, 5, "&");
						    }
					    }
					    links.push_back(link);
				    }
			    }
			    job_ptr->complete(job_ptr->path, job_ptr->status, move(links));
			    using std::chrono::operator""s;
		    } else {
			    job_ptr->complete(job_ptr->path, 0, {});
			    std::cerr << "Error: " << ec << std::endl;
		    }
	    });
}
