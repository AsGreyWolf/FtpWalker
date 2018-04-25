#ifndef WALK_HPP
#define WALK_HPP

#include "connection.hpp"
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
	walk(walk &&);
	walk &operator=(const walk &) = delete;
	walk &operator=(walk &&);
	walk(const host_info &host, const auth_info &auth, callbacks cbks);
	~walk();

private:
	class impl {
		progress_queue<path_info> progress_queue_;
		callbacks callbacks_;
		executor executor_;
		thread_local_storage<std::unique_ptr<connection>> connection_;
		std::function<void(boost::asio::io_service &, path_info path)> executor_task_;
		defer stop_;

	public:
		impl(const host_info &host, const auth_info &auth, callbacks cbks);
	};
	std::unique_ptr<impl> pimpl_;
};

#endif /* end of include guard: WALK_HPP */
