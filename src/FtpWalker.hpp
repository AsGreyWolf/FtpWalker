#ifndef HTTP_WALKER_H
#define HTTP_WALKER_H

// #define BOOST_ASIO_ENABLE_HANDLER_TRACKING 1
#include "types.hpp"
#include "walk.hpp"
#include <QtCore/QObject>

class FtpWalker : public QObject {
	Q_OBJECT

	walk current_walk;

public:
	using HostInfo = host_info;
	using AuthInfo = auth_info;
	struct DescriptorInfo {
		QString name;
		size_t size;
	};
	FtpWalker();
	~FtpWalker();

public slots:
	void start(const HostInfo &, const AuthInfo &);
	void stop();
signals:
	void foundItem(DescriptorInfo ds);
	void started();
	void progress(size_t current, size_t all);
	void finished();
};

#endif /* end of include guard: HTTP_WALKER_H */
