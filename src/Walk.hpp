#ifndef WALK_HPP
#define WALK_HPP

#include <functional>
#include <future>
#include <unordered_set>

class Walk {
	class impl;
	std::unique_ptr<impl> pimpl_;

	std::future<void> task_;
	std::promise<void> to_stop_;

public:
	Walk() = delete;
	Walk(const Walk &) = delete;
	Walk &operator=(const Walk &) = delete;
	Walk(Walk &&second) = default;
	Walk &operator=(Walk &&) = default;

	Walk(std::string url, std::function<void()> started,
	     std::function<void()> finished,
	     std::function<void(size_t, size_t)> progress,
	     std::function<void(std::string, unsigned short)> found);

	~Walk();
};

#endif /* end of include guard: WALK_HPP */
