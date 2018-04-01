#include "Walk.hpp"

#include "job.hpp"
#include <boost/asio.hpp>
// #include <iostream>
#include <mutex>

using std::move;

class Walk::executor {
	std::string host_;
	callbacks cbks_;

	boost::asio::ip::tcp::resolver::endpoint_type endpoint_;
	std::mutex enqueued_mutex_;
	std::unordered_set<std::string> enqueued_;
	std::mutex visited_mutex_;
	std::unordered_set<std::string> visited_;

	boost::asio::io_service *ctx_;
	std::future<void> stop_;

public:
	executor(boost::asio::io_service &ctx, std::string host, callbacks cbks,
	         std::future<void> stop);
	void run();
	void enqueue(const std::string &url);
	~executor();
};
class Walk::executor_task {
	boost::asio::io_service ctx_;
	std::future<void> executor_ftr_;

public:
	executor_task(std::string host, callbacks cbks, std::future<void> stop,
	              const std::string &path)
	    : ctx_{}, executor_ftr_{std::async(
	                  std::launch::async,
	                  [host = move(host), cbks = move(cbks), ftr = move(stop),
	                   path = path, ctx = &ctx_]() mutable {
		                  executor exec{*ctx, move(host), move(cbks), move(ftr)};
		                  exec.enqueue(path);
		                  exec.run();
	                  })} {}
	executor_task(const executor_task &) = delete;
	executor_task(executor_task &&) = delete;
	executor_task &operator=(const executor_task &) = delete;
	executor_task &operator=(executor_task &&) = delete;
	~executor_task() { ctx_.stop(); }
};
Walk::Walk() : executor_task_{}, to_stop_{} {}
Walk::Walk(const std::string &url, callbacks cbks) : to_stop_{} {
	// std::cout << "WALK CONSTRUCTOR" << std::endl;
	std::string host = url.substr(0, url.find('/'));
	std::string path = url.substr(url.find('/'));
	executor_task_ = std::make_unique<executor_task>(move(host), move(cbks),
	                                                 to_stop_.get_future(), path);
}
Walk::Walk(Walk &&) = default;
Walk &Walk::operator=(Walk &&) = default;
Walk::~Walk() {}
Walk::executor::executor(boost::asio::io_service &ctx, std::string host,
                         callbacks cbks, std::future<void> stop)
    : host_{move(host)}, cbks_{move(cbks)}, ctx_{&ctx}, stop_{move(stop)} {
	cbks_.started();
	{
		boost::asio::ip::tcp::resolver resolver{*ctx_};
		auto endpoints = resolver.resolve({host_, "80"});
		boost::asio::ip::tcp::socket tester{*ctx_};
		endpoint_ = connect(tester, endpoints);
		// std::cout << state->endpoint << std::endl;
	}
}
void Walk::executor::run() {
	std::vector<std::future<void>> cotasks(
	    std::max(2u, std::thread::hardware_concurrency()) - 1);
	for (auto &t : cotasks)
		t = std::async(std::launch::async, [ctx = ctx_] { ctx->run(); });
}
Walk::executor::~executor() { cbks_.finished(); }
void Walk::executor::enqueue(const std::string &path) {
	using std::chrono::operator""s;
	if (stop_.wait_for(0s) != std::future_status::timeout) {
		// std::cout << "TIMEOUT " << path << std::endl;
		return;
	}
	{
		std::lock_guard lk{enqueued_mutex_};
		if (enqueued_.find(path) != enqueued_.end())
			return;
		// std::cout << "ENQUEUE " << path << std::endl;
		enqueued_.insert(path);
		cbks_.progress(visited_.size(), enqueued_.size());
	}
	auto job_ptr = std::make_unique<job>(
	    *ctx_, host_, path,
	    [this](const std::string &path, unsigned short status,
	           std::vector<std::string> links) {
		    {
			    std::lock_guard lk{visited_mutex_};
			    visited_.insert(path);
			    // std::cout << "FOUND " << path <<
			    // std::endl;
			    cbks_.progress(visited_.size(), enqueued_.size());
		    }
		    cbks_.found(path, status);
		    for (auto &link : links)
			    enqueue(link);
	    });
	connect(move(job_ptr), endpoint_);
}
