#pragma once

#include <cstddef>

namespace webserv {

// Process-wide admission limits, shared by all configured listeners.
// These bound job/client counts, not total memory or response-file size.
constexpr std::size_t MAX_ACTIVE_CLIENTS = 128;
constexpr std::size_t MAX_CGI_JOBS = 16;
constexpr int ACCEPT_RETRY_MILLISECONDS = 250;

} // namespace webserv
