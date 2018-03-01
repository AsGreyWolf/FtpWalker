#include "Walk.hpp"
#include "HttpWalker.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <mutex>
#include <regex>

using std::move;

struct Walk::impl {
	HttpWalker *emitter;
	std::string host;

	boost::asio::ip::tcp::resolver::endpoint_type endpoint;
	std::mutex enqueued_mutex;
	std::unordered_set<std::string> enqueued;
	std::mutex visited_mutex;
	std::unordered_set<std::string> visited;

	boost::asio::io_service ctx;
	std::future<void> stop;
	void enqueue(const std::string &url);
};

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

Walk::Walk(HttpWalker *emitter, std::string url)
    : pimpl_{std::make_unique<Walk::impl>()} {
	// std::cout << "WALK CONSTRUCTOR" << std::endl;
	pimpl_->emitter = emitter;
	pimpl_->host = url.substr(0, url.find('/'));
	std::string path = url.substr(url.find('/'));
	pimpl_->stop = to_stop_.get_future();
	task_ = std::async(
	    std::launch::async, [ state = pimpl_.get(), path = move(path) ] {
		    {
			    boost::asio::ip::tcp::resolver resolver{state->ctx};
			    auto endpoints = resolver.resolve({state->host, "80"});
			    boost::asio::ip::tcp::socket tester{state->ctx};
			    state->endpoint = connect(tester, endpoints);
			    std::cout << state->endpoint << std::endl;
		    }
		    state->enqueue(path);
		    std::vector<std::future<void>> cotasks(
		        std::max(2u, std::thread::hardware_concurrency()) - 1);
		    for (auto &t : cotasks)
			    t = std::async(std::launch::async, [&ctx = state->ctx] { ctx.run(); });
	    });
}
Walk::~Walk() {}

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
void Walk::impl::enqueue(const std::string &raw_path) {
	std::string path = collapse(raw_path);
	{
		std::lock_guard lk{enqueued_mutex};
		if (enqueued.find(path) != enqueued.end())
			return;
		using std::chrono::operator""s;
		if (stop.wait_for(0s) != std::future_status::timeout) {
			// std::cout << "TIMEOUT " << path << std::endl;
			return;
		}
		// std::cout << "ENQUEUE " << path << std::endl;
		enqueued.insert(path);
		emit emitter->progress(visited.size(), enqueued.size());
	}
	auto job_ptr = std::make_unique<job>(
	    ctx, host, path,
	    [this](const std::string &path, unsigned short status,
	           std::vector<std::string> links) {
		    {
			    std::lock_guard lk{visited_mutex};
			    visited.insert(path);
			    // std::cout << "FOUND " << path <<
			    // std::endl;
		    }
		    emit emitter->foundItem(QString(path.c_str()), status);
		    for (auto &link : links)
			    enqueue(link);
	    });
	connect_job(move(job_ptr), endpoint);
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
