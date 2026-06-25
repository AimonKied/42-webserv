#include "EventLoop.hpp"

namespace webserv {

EventLoop::EventLoop() : servers(), clients() {
}

EventLoop::~EventLoop() {
}

void EventLoop::run() {
    // TODO: implement polling and socket management
}

void EventLoop::addServer(const Server &server) {
    servers.push_back(server);
}

} // namespace webserv
