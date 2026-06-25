#include "Server.hpp"

namespace webserv {

Server::Server()
    : host("0.0.0.0"), port(8080), root("./www"), routes(), listenSocket(-1) {
}

Server::~Server() {
}

} // namespace webserv
