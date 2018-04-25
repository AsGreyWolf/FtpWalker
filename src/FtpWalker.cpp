#include "FtpWalker.hpp"
#include <memory>

FtpWalker::FtpWalker() : current_walk{} {}
FtpWalker::~FtpWalker() {}

void FtpWalker::stop() { current_walk = {}; }
void FtpWalker::start(const HostInfo &host, const AuthInfo &auth) {
	current_walk =
	    walk{host,
	         auth,
	         {[this] { emit started(); }, [this] { emit finished(); },
	          [this](size_t cur, size_t all) { emit progress(cur, all); },
	          [this](descriptor_info descr) {
		          emit foundItem(QString(descr.name.c_str()), descr.size);
	          }}};
}
