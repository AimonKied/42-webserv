# Architecture

This documentation will explain the webserv architecture as the project grows.

## Goals

- Keep components small and easy to understand.
- Separate networking, parsing, and response logic.
- Enable incremental implementation.

## Socket I/O policy

The bundled subject forbids using errno to choose server behavior after a read
or write. Socket operations run after poll readiness and use their return value:

- A positive recv appends bytes; zero (EOF) or a negative result closes the client.
- A positive send removes only the bytes sent and retains the remainder for a
  later POLLOUT. A negative result, or zero progress on nonempty output, closes
  the client. Empty output is closed without issuing a send.
- There is no errno-based EAGAIN/EINTR retry after socket I/O. This is a deliberate
  subject-specific policy: even a transient syscall failure ends that connection.
- errno checks for poll, accept and waitpid remain separate from read/write policy.

Run `python3 tests/socket_integration.py ./webserv` after building. It uses temporary
files and two local ports to test fragmented requests, paused readers, complete
large transfers, concurrent requests, reset connections, descriptor cleanup (on
Linux), and shutdown. CGI ownership and routing are described in `cgi-runtime.md`.
