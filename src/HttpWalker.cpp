#include "HttpWalker.hpp"
#include "Walk.hpp"
#include <memory>

HttpWalker::HttpWalker() {}
HttpWalker::~HttpWalker() {}

void HttpWalker::stop() { current_walk = {}; }
void HttpWalker::start(const std::string &url) {
	current_walk = std::make_unique<Walk>(this, url);
}
