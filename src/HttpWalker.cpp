#include "HttpWalker.hpp"
#include "Walk.hpp"
#include <memory>

HttpWalker::HttpWalker() {}
HttpWalker::~HttpWalker() {}

void HttpWalker::stop() { current_walk = {}; }
void HttpWalker::start(const std::string &url) {
	current_walk = std::make_unique<Walk>(
	    url, [this] { emit started(); }, [this] { emit finished(); },
	    [this](size_t cur, size_t all) { emit progress(cur, all); },
	    [this](std::string path, unsigned short code) {
		    emit foundItem(QString(path.c_str()), code);
	    });
}
