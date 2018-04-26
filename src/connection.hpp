#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "types.hpp"
#include <algorithm>
#include <boost/asio.hpp>
#include <iterator>
#include <regex>
#include <string>

class connection {
	boost::asio::io_service *context_;
	boost::asio::ip::tcp::socket control_port_;

	const std::string eol = "\r\n";
	std::vector<std::string> read_transfer(boost::asio::ip::tcp::socket &socket) {
		std::vector<std::string> result;
		boost::asio::streambuf input;
		std::istream is{&input};
		while (true) {
			boost::system::error_code ec;
			size_t input_len = boost::asio::read_until(socket, input, eol, ec);
			if (ec == boost::asio::error::eof)
				break;
			result.emplace_back();
			result.back().reserve(input_len);
			std::copy_n(std::istreambuf_iterator<char>{is}, input_len - eol.size(),
			            std::back_inserter(result.back()));
			// std::cout << result.back() << std::endl;
			is.ignore(eol.size() + 1);
		}
		return result;
	}
	std::pair<int, std::string> read_answer() {
		std::string result;
		boost::asio::streambuf input;
		size_t input_len = boost::asio::read_until(control_port_, input, eol);
		std::istream is{&input};
		result.reserve(input_len);
		std::copy_n(std::istreambuf_iterator<char>{is}, input_len - eol.size(),
		            std::back_inserter(result));

		// std::cout << "\t<<< " << result << std::endl;
		const std::regex answer_parser{R"R((\d{3})\s*(.*))R",
		                               std::regex_constants::ECMAScript |
		                                   std::regex_constants::optimize};
		std::smatch match;
		if (!std::regex_match(result, match, answer_parser))
			throw std::logic_error{"Invalid answer"};
		return {std::stoi(match[1]), match[2]};
	}
	std::string &&throw_if_invalid(int required_status,
	                               std::pair<int, std::string> &&answer) const {
		auto &&[status, msg] = answer;
		if (required_status != status)
			throw std::logic_error{msg};
		return move(msg);
	}
	auto ask(int required_status, std::string msg) {
		// std::cout << ">>> " << msg << std::endl;
		msg += "\r\n";
		boost::asio::write(control_port_, boost::asio::buffer(msg));
		return throw_if_invalid(required_status, read_answer());
	}
	auto connect_transfer() {
		auto transfer_info_answer = ask(227, "PASV");
		const std::regex ip_port_parser{
		    R"R(.*\((\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3}),(\d{1,3})\).*)R",
		    std::regex_constants::ECMAScript | std::regex_constants::optimize};
		std::smatch match;
		if (!std::regex_match(transfer_info_answer, match, ip_port_parser))
			throw std::logic_error("Invalid answer");
		boost::asio::ip::tcp::socket transfer_port{*context_};
		uint32_t ip = 0;
		for (int i = 0; i < 4; i++)
			ip |= (stoi(match[i + 1]) << (8 * (3 - i)));
		uint16_t port = 0;
		for (int i = 0; i < 2; i++)
			port |= (stoi(match[i + 5]) << (8 * (1 - i)));
		transfer_port.connect({boost::asio::ip::address_v4(ip), port});
		return transfer_port;
	}
	std::string path_concat(std::string a, const std::string &b) {
		auto result = move(a);
		if (result.back() != '/')
			result += '/';
		result += b;
		return result;
	}

public:
	connection(boost::asio::io_service &ctx, const host_info &host,
	           const auth_info &auth)
	    : context_{&ctx}, control_port_{*context_} {
		boost::asio::ip::tcp::resolver resolver{*context_};
		auto endpoints = resolver.resolve({host.address, std::to_string(host.port)});
		boost::asio::connect(control_port_, endpoints);
		throw_if_invalid(220, read_answer());
		ask(331, "USER " + auth.login);
		ask(230, "PASS " + auth.password);
	};

	std::vector<descriptor_info> folders(const std::string &path) {
		std::vector<descriptor_info> result;
		auto transfer_port = connect_transfer();
		ask(150, "LIST " + path);
		std::regex folder_parser{
		    R"R(d.{9}\s+(?:\S*\s+){3}(\d*).*?('.+?'|".+?"|[^\s'"]+)\n)R",
		    std::regex_constants::ECMAScript | std::regex_constants::optimize};
		for (auto &&line : read_transfer(transfer_port)) {
			line += '\n';
			std::smatch match;
			if (std::regex_match(line, match, folder_parser))
				if (match[2] != ".." && match[2] != ".")
					result.push_back({path_concat(path, match[2]), std::stoull(match[1])});
		}
		throw_if_invalid(226, read_answer());
		return result;
	}
	std::vector<descriptor_info> files(const std::string &path) {
		std::vector<descriptor_info> result;
		auto transfer_port = connect_transfer();
		ask(150, "LIST " + path);
		std::regex folder_parser{
		    R"R(-.{9}\s+(?:\S*\s+){3}(\d*).*?('.+?'|".+?"|[^\s'"]+)\n)R",
		    std::regex_constants::ECMAScript | std::regex_constants::optimize};
		for (auto &&line : read_transfer(transfer_port)) {
			line += '\n';
			std::smatch match;
			if (std::regex_match(line, match, folder_parser))
				result.push_back({path_concat(path, match[2]), std::stoull(match[1])});
		}
		throw_if_invalid(226, read_answer());
		return result;
	}
};

#endif /* end of include guard: CONNECTION_HPP */
