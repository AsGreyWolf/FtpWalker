#ifndef WALK_HPP
#define WALK_HPP

#include <future>
#include <unordered_set>

class HttpWalker;
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

	Walk(HttpWalker *emitter, std::string url);

	~Walk();
};

#endif /* end of include guard: WALK_HPP */