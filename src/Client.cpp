#include "Client.hpp"

namespace webserv {

Client::Client(int socketFd)
    : socketFd(socketFd), readBuffer(), writeBuffer(), state(ClientState::Reading), request(), response() {
}

Client::~Client() {
}

} // namespace webserv
