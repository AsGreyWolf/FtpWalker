#include "FtpWalker.hpp"
#include <memory>

FtpWalker::FtpWalker() : current_walk{} {}
FtpWalker::~FtpWalker() {}

void FtpWalker::stop() { current_walk = {}; }
void FtpWalker::start(const std::string &url) {
	current_walk =
	    Walk{url,
	         {[this] { emit started(); }, [this] { emit finished(); },
	          [this](size_t cur, size_t all) { emit progress(cur, all); },
	          [this](std::string path, size_t size) {
		          emit foundItem(QString(path.c_str()), size);
	          }}};
}
