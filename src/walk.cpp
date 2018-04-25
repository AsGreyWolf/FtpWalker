#include "walk.hpp"

// #include "job.hpp"

using tcp = boost::asio::ip::tcp;
using std::move;

std::unique_ptr<walk::ftp_executor>
make_executor(const host_info &host, const auth_info &auth,
              std::function<void(size_t, size_t)> progress,
              std::function<void(descriptor_info)> found) {
	auto submit = [host = host, found = move(found),
	               endpoint = tcp::resolver::endpoint_type{}](
	                  boost::asio::io_service &ctx, path_info el,
	                  std::function<void(path_info)> enqueue) mutable {
		{
			tcp::resolver resolver{ctx};
			auto endpoints = resolver.resolve({host.address, std::to_string(host.port)});
			tcp::socket tester{ctx};
			endpoint = connect(tester, endpoints);
		}
		// auto job_ptr = std::make_unique<job>(
		//     *ctx_, auth_, path,
		//     [this](descriptor_info descr, std::vector<std::string> folders) {
		// 	    {
		// 		    std::lock_guard lk{visited_mutex_};
		// 		    visited_.insert(descr.name);
		// 		    // std::cout << "FOUND " << path <<
		// 		    // std::endl;
		// 		    cbks_.progress(visited_.size(), enqueued_.size());
		// 	    }
		// 	    cbks_.found(move(descr));
		// 	    for (auto &link : folders)
		// 		    enqueue(link);
		//     });
		// connect(move(job_ptr), endpoint_);
	};
	return std::make_unique<walk::ftp_executor>(
	    "/", walk::ftp_executor::callbacks{move(progress), move(submit)});
}

walk::walk(const host_info &host, const auth_info &auth, callbacks cbks)
    : executor_task_{make_executor(host, auth, move(cbks.progress),
                                   move(cbks.found))},
      stop_{move(cbks.stop)} {
	cbks.start();
}
