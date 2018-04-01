#include "HttpWalker.hpp"
#include <memory>

HttpWalker::HttpWalker() : current_walk{} {}
HttpWalker::~HttpWalker() {}

void HttpWalker::stop() { current_walk = {}; }
void HttpWalker::start(const std::string &url) {
	current_walk =
	    Walk{url,
	         {[this] { emit started(); }, [this] { emit finished(); },
	          [this](size_t cur, size_t all) { emit progress(cur, all); },
	          [this](std::string path, unsigned short code) {
		          emit foundItem(QString(path.c_str()), code);
	          }}};
}
