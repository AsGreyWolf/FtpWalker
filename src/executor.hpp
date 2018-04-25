#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <boost/asio.hpp>
#include <functional>
#include <mutex>
#include <unordered_set>

template <typename Element> class progress_queue {
	std::mutex enqueued_mutex_{};
	std::unordered_set<Element> enqueued_{};
	std::mutex visited_mutex_{};
	std::unordered_set<Element> visited_{};

public:
	using value_type = Element;
	bool push(Element el) {
		std::lock_guard lk{enqueued_mutex_};
		if (enqueued_.find(el) != enqueued_.end())
			return false;
		enqueued_.insert(move(el));
		return true;
	}
	void visit(Element el) {
		std::lock_guard lk{visited_mutex_};
		if (visited_.find(el) != visited_.end())
			return;
		visited_.insert(move(el));
	}
	size_t enqueued() {
		std::lock_guard lk{enqueued_mutex_};
		return enqueued_.size();
	}
	size_t visited() {
		std::lock_guard lk{visited_mutex_};
		return visited_.size();
	}
};

template <typename Element> class executor {
public:
	using index_type = Element;
	struct callbacks {
		std::function<void(size_t, size_t)> progress;
		std::function<void(boost::asio::io_service &, Element,
		                   std::function<void(Element)>)>
		    submit;
	};

	executor(Element root, callbacks cbks)
	    : progress_{}, cbks_{std::move(cbks)}, ctx_{},
	      task_{std::async(std::launch::async, [this, root = move(root)] {
		      enqueue(move(root));
		      run();
	      })} {}
	~executor() { stop(); }

private:
	progress_queue<index_type> progress_;
	callbacks cbks_;
	boost::asio::io_service ctx_;
	std::future<void> task_;

	void enqueue(Element el) {
		progress_.push(el);
		cbks_.progress(progress_.visited(), progress_.enqueued());
		cbks_.submit(ctx_, move(el), [this](Element el) {
			enqueue(move(el));
			progress_.visit(el);
		});
	}
	void run() {
		std::vector<std::future<void>> cotasks(
		    std::max(2u, std::thread::hardware_concurrency()) - 1);
		for (auto &t : cotasks)
			t = std::async(std::launch::async, [this] { ctx_.run(); });
	}
	void stop() { ctx_.stop(); }
};

#endif /* end of include guard: EXECUTOR_HPP */
