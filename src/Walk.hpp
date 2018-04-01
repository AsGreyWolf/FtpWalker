#ifndef WALK_HPP
#define WALK_HPP

#include <functional>
#include <future>
#include <unordered_set>

class Walk {
	class executor;
	class executor_task;
	std::unique_ptr<executor_task> executor_task_;
	std::promise<void> to_stop_;

public:
	struct callbacks {
		std::function<void()> started;
		std::function<void()> finished;
		std::function<void(size_t, size_t)> progress;
		std::function<void(std::string, unsigned short)> found;
	};

	Walk();
	Walk(const Walk &) = delete;
	Walk(Walk &&);
	Walk &operator=(const Walk &) = delete;
	Walk &operator=(Walk &&);
	Walk(const std::string &url, callbacks cbks);
	~Walk();
};

#endif /* end of include guard: WALK_HPP */
