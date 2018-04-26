#ifndef WALK_HPP
#define WALK_HPP

#include "types.hpp"
#include <functional>
#include <memory>

class walk {
public:
	struct callbacks {
		std::function<void()> start;
		std::function<void()> stop;
		std::function<void(size_t, size_t)> progress;
		std::function<void(descriptor_info)> found;
	};
	walk();
	walk(const host_info &host, const auth_info &auth, callbacks cbks);
	walk(const walk &) = delete;
	walk(walk &&) = default;
	walk &operator=(const walk &) = delete;
	walk &operator=(walk &&);
	~walk();

private:
	class impl;
	std::unique_ptr<impl> pimpl_;
};

#endif /* end of include guard: WALK_HPP */
