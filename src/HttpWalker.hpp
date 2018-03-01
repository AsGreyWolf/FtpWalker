#ifndef HTTP_WALKER_H
#define HTTP_WALKER_H

// #define BOOST_ASIO_ENABLE_HANDLER_TRACKING 1
#include <QtCore/QObject>
#include <functional>
#include <memory>
#include <string>

class Walk;
class HttpWalker : public QObject {
	Q_OBJECT

	std::unique_ptr<Walk> current_walk;

public:
	HttpWalker();
	~HttpWalker();

public slots:
	void start(const std::string &url);
	void stop();
signals:
	void foundItem(QString url, unsigned short code);
	void progress(size_t current, size_t all);
};

#endif /* end of include guard: HTTP_WALKER_H */
