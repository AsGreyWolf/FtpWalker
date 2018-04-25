#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <boost/asio.hpp>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

template <typename Element> class progress_queue {
	std::mutex enqueued_mutex_{};
	std::unordered_set<Element> enqueued_{};
	std::mutex visited_mutex_{};
	std::unordered_set<Element> visited_{};

public:
	using value_type = Element;
	bool push(Element el) {
		std::lock_guard<std::mutex> lk{enqueued_mutex_};
		if (enqueued_.find(el) != enqueued_.end())
			return false;
		enqueued_.insert(move(el));
		return true;
	}
	void visit(Element el) {
		std::lock_guard<std::mutex> lk{visited_mutex_};
		if (visited_.find(el) != visited_.end())
			return;
		visited_.insert(move(el));
	}
	size_t enqueued() {
		std::lock_guard<std::mutex> lk{enqueued_mutex_};
		return enqueued_.size();
	}
	size_t visited() {
		std::lock_guard<std::mutex> lk{visited_mutex_};
		return visited_.size();
	}
};

template <typename T> class thread_local_storage {
	std::mutex per_thread_data_mutex_;
	std::unordered_map<std::thread::id, T> per_thread_data_;

public:
	using value_type = T;
	explicit operator value_type &() {
		std::lock_guard<std::mutex> mtx{per_thread_data_mutex_};
		return per_thread_data_[std::this_thread::get_id()];
	}
	explicit operator value_type &() const {
		std::lock_guard<std::mutex> mtx{per_thread_data_mutex_};
		return per_thread_data_[std::this_thread::get_id()];
	}
};

class executor {
public:
	using task_type = std::function<void(boost::asio::io_service &)>;

	executor() = default;
	executor(const executor &) = delete;
	executor(executor &&) = delete;
	executor &operator=(const executor &) = delete;
	executor &operator=(executor &&) = delete;
	~executor() { stop(); }
	void start(task_type root) {
		task_ = std::async(std::launch::async, [this, root = move(root)] {
			root(ctx_);
			run();
		});
	}
	void finish() {
		stop();
		task_.wait();
	}

private:
	boost::asio::io_service ctx_{};
	std::future<void> task_{};
	void run() {
		std::vector<std::future<void>> cotasks(
		    std::max(1u, std::thread::hardware_concurrency()));
		for (auto &t : cotasks)
			t = std::async(std::launch::async, [this] { ctx_.run(); });
	}
	void stop() { ctx_.stop(); }
};

#endif /* end of include guard: EXECUTOR_HPP */
