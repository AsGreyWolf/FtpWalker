#include "job.hpp"

#include <iostream>
#include <regex>

void job::connect(std::unique_ptr<job> job_ptr,
                  boost::asio::ip::tcp::resolver::endpoint_type endpoint) {
	// std::cout << "CONNECT " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	raw_ptr->socket.async_connect(
	    endpoint, [job_ptr = move(job_ptr)](boost::system::error_code ec) mutable {
		    if (!ec) {
			    send(move(job_ptr));
		    }
	    });
}
void job::send(std::unique_ptr<job> job_ptr) {
	// std::cout << "SEND " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	std::ostream os{&raw_ptr->write_buffer};
	os << "GET " << raw_ptr->path << " HTTP/1.0\r\n"
	   << "Host: " << raw_ptr->host << "\r\n"
	   << "Accept: */*\r\n"
	   << "Connection: close\r\n\r\n";
	boost::asio::async_write(
	    raw_ptr->socket, raw_ptr->write_buffer,
	    [job_ptr = move(job_ptr)](boost::system::error_code ec,
	                              std::size_t bytes_transferred) mutable {
		    if (!ec) {
			    read_status(move(job_ptr));
		    }
	    });
}
void job::read_status(std::unique_ptr<job> job_ptr) {
	// std::cout << "READ_STATUS " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	boost::asio::async_read_until(
	    raw_ptr->socket, raw_ptr->read_buffer, "\r\n",
	    [job_ptr = move(job_ptr)](boost::system::error_code ec,
	                              std::size_t lem) mutable {
		    if (!ec) {
			    std::string version, code_msg;
			    unsigned short code;
			    std::istream is{&job_ptr->read_buffer};
			    is >> version >> code;
			    std::getline(is, code_msg);
			    if (is && version.substr(0, 5) == "HTTP/") {
				    job_ptr->status = code;
				    return recieve(move(job_ptr));
			    }
		    }
		    job_ptr->complete(job_ptr->path, 0, {});
		    std::cerr << "Error: " << ec << std::endl;
	    });
}
void job::recieve(std::unique_ptr<job> job_ptr) {
	// std::cout << "READ_CONTENT " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	boost::asio::async_read(
	    raw_ptr->socket, raw_ptr->read_buffer,
	    [job_ptr = move(job_ptr)](boost::system::error_code ec,
	                              std::size_t lem) mutable {
		    if (!ec) {
			    recieve(move(job_ptr));
		    } else if (ec == boost::asio::error::eof) {
			    std::vector<std::string> links;
			    std::string buf(std::istreambuf_iterator<char>{&job_ptr->read_buffer},
			                    {});
			    std::regex ahref{"<\\s*a.*href\\s*=\\s*(\"[^\"]*\"|\'[^\']*\'|\\S*)"};
			    auto begin = std::sregex_iterator(buf.begin(), buf.end(), ahref);
			    auto end = std::sregex_iterator{};
			    for (auto it = begin; it != end; it++) {
				    std::string link = (*it)[1];
				    if (link[0] == '\'' || link[0] == '"')
					    link = link.substr(1, link.size() - 2);
				    std::regex external{"\\w*:.*"};
				    if (!std::regex_match(link, external)) {
					    if (link[0] != '/')
						    link = job_ptr->path.substr(0, job_ptr->path.rfind('/') + 1) + link;
					    size_t param_pos = link.find('?');
					    if (param_pos != std::string::npos) {
						    size_t pos = param_pos;
						    while (true) {
							    pos = link.find("&amp;", pos + 1);
							    if (pos == std::string::npos)
								    break;
							    link.replace(pos, 5, "&");
						    }
					    }
					    links.push_back(link);
				    }
			    }
			    job_ptr->complete(job_ptr->path, job_ptr->status, move(links));
			    using std::chrono::operator""s;
		    } else {
			    job_ptr->complete(job_ptr->path, 0, {});
			    std::cerr << "Error: " << ec << std::endl;
		    }
	    });
}
