#ifndef WALK_HPP
#define WALK_HPP

#include "defer.hpp"
#include "executor.hpp"
#include "types.hpp"
#include <functional>

class walk {
public:
	struct callbacks {
		std::function<void()> start;
		std::function<void()> stop;
		std::function<void(size_t, size_t)> progress;
		std::function<void(descriptor_info)> found;
	};
	walk() = default;
	walk(const walk &) = delete;
	walk(walk &&) = default;
	walk &operator=(const walk &) = delete;
	walk &operator=(walk &&) = default;
	walk(const host_info &host, const auth_info &auth, callbacks cbks);

private:
	using ftp_executor = executor<path_info>;

	std::unique_ptr<ftp_executor> executor_task_;
	defer stop_;

	friend std::unique_ptr<ftp_executor>
	make_executor(const host_info &host, const auth_info &auth,
	              std::function<void(size_t, size_t)> progress,
	              std::function<void(descriptor_info)> found);
};

#endif /* end of include guard: WALK_HPP */
