#include "walk.hpp"

using std::move;

walk::impl::impl(const host_info &host, const auth_info &auth, callbacks cbks)
    : progress_queue_{}, callbacks_{move(cbks)}, executor_{}, connection_{},
      executor_task_{[this, host, auth](boost::asio::io_service &ctx,
                                        path_info path) {
	      progress_queue_.push(path);
	      auto &conn_ptr = static_cast<std::unique_ptr<connection> &>(connection_);
	      if (!conn_ptr)
		      conn_ptr = std::make_unique<connection>(ctx, host, auth);
	      for (auto &&file : conn_ptr->files(path)) {
		      progress_queue_.push(file.name);
		      progress_queue_.visit(file.name);
		      callbacks_.found(move(file));
	      }
	      for (auto &&dir : conn_ptr->folders(path)) {
		      boost::asio::dispatch(
		          ctx, [&ctx, dir, this] { executor_task_(ctx, dir.name); });
		      callbacks_.found(move(dir));
	      }
	      progress_queue_.visit(move(path));
	      callbacks_.progress(progress_queue_.visited(),
	                          progress_queue_.enqueued());
      }},
      stop_{[this] { callbacks_.stop(); }} {
	executor_.start([this](boost::asio::io_service &ctx) {
		callbacks_.start();
		executor_task_(ctx, "/");
	});
}

walk::walk(const host_info &host, const auth_info &auth, callbacks cbks)
    : pimpl_{std::make_unique<walk::impl>(host, auth, move(cbks))} {}
walk::walk(walk &&) = default;
walk &walk::operator=(walk &&) = default;
walk::~walk() {}
