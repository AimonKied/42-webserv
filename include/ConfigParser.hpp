#pragma once

#include "ConfigTypes.hpp"
#include <cstddef>
#include <string>
#include <vector>

class ConfigParser {
public:
	ConfigParser();
	~ConfigParser();

	Config parseFile(const std::string &path) const;

private:
	LocationConfig parseLocation(const std::vector<std::string>& tokens,
								 std::size_t& position) const;
	int parsePort(const std::string& value) const;
	std::size_t parseBodySize(const std::string& value) const;
	void validateIPv4(const std::string& address) const;
	std::string take(const std::vector<std::string>& tokens, std::size_t& position) const;
	void expect(const std::vector<std::string>& tokens, std::size_t& position,
				const std::string& expected) const;
};
