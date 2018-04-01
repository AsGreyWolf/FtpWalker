#ifndef JOB_HPP
#define JOB_HPP

#include <boost/asio.hpp>
#include <functional>
#include <map>
#include <string>

struct job {
	boost::asio::ip::tcp::socket socket;
	std::string host;
	std::string path;
	std::function<void(const std::string &url, unsigned short status,
	                   std::vector<std::string> links)>
	    complete;
	std::map<std::string, std::string> header;
	boost::asio::streambuf write_buffer;
	boost::asio::streambuf read_buffer;
	unsigned short status = -1;

	job(boost::asio::io_service &ctx_, std::string host_, std::string path_,
	    std::function<void(const std::string &url, unsigned short status,
	                       std::vector<std::string> links)>
	        complete_)
	    : socket{ctx_}, host{host_}, path{move(path_)}, complete{move(complete_)} {
	}
};

void connect(std::unique_ptr<job> job_ptr,
             boost::asio::ip::tcp::resolver::endpoint_type endpoints);
void send(std::unique_ptr<job> job_ptr);
void read_status(std::unique_ptr<job> job_ptr);
void read_header(std::unique_ptr<job> job_ptr);
void recieve(std::unique_ptr<job> job_ptr);

#endif /* end of include guard: JOB_HPP */
