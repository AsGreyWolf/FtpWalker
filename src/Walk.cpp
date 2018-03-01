#include "Walk.hpp"
#include "HttpWalker.hpp"
#include "job.hpp"
#include <boost/asio.hpp>
// #include <iostream>
#include <mutex>

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
			    // std::cout << state->endpoint << std::endl;
		    }
		    state->enqueue(path);
		    std::vector<std::future<void>> cotasks(
		        std::max(2u, std::thread::hardware_concurrency()) - 1);
		    for (auto &t : cotasks)
			    t = std::async(std::launch::async, [&ctx = state->ctx] { ctx.run(); });
	    });
}
Walk::~Walk() {}

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
	job::connect(move(job_ptr), endpoint);
}
