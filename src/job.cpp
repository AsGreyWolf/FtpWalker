#include "job.hpp"

// #include <iostream>
#include <regex>

std::string collapse(std::string s) {
	s = s.substr(0, s.find('#'));
	while (true) {
		size_t pos = s.find("/../");
		if (pos == std::string::npos)
			break;
		if (pos == 0)
			break;
		size_t start = s.rfind('/', pos - 1);
		if (start == std::string::npos)
			break;
		s.replace(start, pos + 3 - start, "");
	}
	return s;
}
std::regex re_ext{"(\\w*):(?:\\/\\/([\\w.]*)(.*))?.*"};
std::string fix_link(job *job_ptr, std::string link, bool &external) {
	std::smatch m;
	external = std::regex_match(link, m, re_ext);
	if (external && m[1] == "http" && m[2] == job_ptr->host) {
		external = false;
		link = m[3];
	}
	external |= link.find(' ') != std::string::npos;
	if (!external) {
		if (link[0] != '/') {
			while (link.substr(0, 2) == "./")
				link = link.substr(2);
			link = job_ptr->path.substr(0, job_ptr->path.rfind('/') + 1) + link;
		}
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
		link = collapse(link);
	}
	return link;
}
void job::connect(std::unique_ptr<job> job_ptr,
                  boost::asio::ip::tcp::resolver::endpoint_type endpoint) {
	// std::cout << "CONNECT " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	raw_ptr->socket.async_connect(endpoint,
	                              [ job_ptr = move(job_ptr), endpoint ](
	                                  boost::system::error_code ec) mutable {
		                              if (!ec) {
			                              return send(move(job_ptr));
		                              }
		                              return job::connect(move(job_ptr), endpoint);
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
			    return read_status(move(job_ptr));
		    }
		    // std::cerr << "Error: " << ec << std::endl;
		    return job_ptr->complete(job_ptr->path, 2, {});
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
				    return read_header(move(job_ptr));
			    }
		    }
		    // std::cerr << "Error: " << ec << std::endl;
		    return job_ptr->complete(job_ptr->path, 0, {});
	    });
}
std::regex re_header_line{R"R(([^:]*):\s*([^;]*)(?:;\s*(.*))?\r)R"};
void job::read_header(std::unique_ptr<job> job_ptr) {
	// std::cout << "READ_HEADER " << job_ptr->path << std::endl;
	auto raw_ptr = job_ptr.get();
	boost::asio::async_read_until(
	    raw_ptr->socket, raw_ptr->read_buffer, "\r\n\r\n",
	    [job_ptr = move(job_ptr)](boost::system::error_code ec,
	                              std::size_t lem) mutable {
		    if (!ec) {
			    std::istream is{&job_ptr->read_buffer};
			    std::string line;
			    while (std::getline(is, line)) {
				    if (line == "\r")
					    break;
				    std::smatch m;
				    if (std::regex_match(line, m, re_header_line)) {
					    job_ptr->header[m[1]] = m[2];
				    } else {
				    }
			    }
			    if (job_ptr->status >= 300 && job_ptr->status < 400) {
				    std::vector<std::string> links;
				    if (!job_ptr->header["Location"].empty())
					    links.push_back(job_ptr->header["Location"]);
				    if (!job_ptr->header["Content-Location"].empty())
					    links.push_back(job_ptr->header["Content-Location"]);
				    std::vector<std::string> internal_links;
				    for (auto link : links) {
					    bool external;
					    link = fix_link(job_ptr.get(), job_ptr->header["Location"], external);
					    if (!external)
						    internal_links.push_back(link);
				    }
				    return job_ptr->complete(job_ptr->path, job_ptr->status,
				                             move(internal_links));
			    }
			    if (job_ptr->header["Content-Type"] == "text/html") {
				    return recieve(move(job_ptr));
			    }
			    return job_ptr->complete(job_ptr->path, job_ptr->status, {});
		    }
		    // std::cerr << "Error: " << ec << std::endl;
		    return job_ptr->complete(job_ptr->path, 1, {});
	    });
}
std::regex re_ahref{
    R"R(<a(?:\s|\s[^>]*\s)href\s*=\s*("[^>"]*"|'[^>']*'|[^\s>]*)[^>]*>)R"};
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
			    auto begin = std::sregex_iterator(buf.begin(), buf.end(), re_ahref);
			    auto end = std::sregex_iterator{};
			    for (auto it = begin; it != end; it++) {
				    std::string link = (*it)[1];
				    if (link[0] == '\'' || link[0] == '"')
					    link = link.substr(1, link.size() - 2);
				    bool external;
				    link = fix_link(job_ptr.get(), link, external);
				    if (!external)
					    links.push_back(link);
			    }
			    return job_ptr->complete(job_ptr->path, job_ptr->status, move(links));
		    }
		    // std::cerr << "Error: " << ec << std::endl;
		    return job_ptr->complete(job_ptr->path, 0, {});
	    });
}
