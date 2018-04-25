#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>

struct auth_info {
	std::string login;
	std::string password;
};
struct host_info {
	std::string address;
	uint16_t port;
};
struct descriptor_info {
	std::string name;
	size_t size;
};
using path_info = std::string;

#endif /* end of include guard: TYPES_HPP */
