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

## Overload and admission

`RuntimeLimits.hpp` defines process-wide limits shared by all listeners:

- 128 accepted clients. At capacity, listening descriptors are disabled only in
  the poll snapshot (fd -1). Existing clients and CGI pipes continue to progress.
  Accepting resumes when a client closes or times out. Pending connections wait
  in the kernel backlog; they may time out or be refused if the backlog fills.
  No HTTP response is promised for a connection that has not been accepted.
- 16 tracked CGI jobs. A full executor returns `CgiStartResult::Unavailable`
  before allocating pipes or forking. The server sends 503 with `Retry-After: 1`
  through its ordinary POLLOUT response path. Cancelled jobs occupy a slot until
  reaped, so disconnections cannot bypass this limit. Static service is unaffected
  by CGI saturation as long as a client slot is available.
- A 250-ms accept cooldown after resource exhaustion or other persistent accept
  errors. All listeners pause together because process resources are shared. The
  poll timeout is shortened to retry when the cooldown expires; there is no sleep
  or blocking retry loop. Capacity/cooldown are rechecked before each accept to
  handle multiple ready listeners in the same snapshot.

EAGAIN/EWOULDBLOCK, EINTR and aborted pending connections from accept are transient.
EBADF, ENOTSOCK and EINVAL indicate a broken listener and stop the runtime with
cleanup. Other accept failures enter the cooldown. Resource errors from pipe/fork
map to CGI 503; other setup failures remain 502. These errno checks happen after
accept/pipe/fork, never after socket or pipe reads/writes.

These are count limits, not a global memory budget. Request limits remain
configuration-driven and static responses are still buffered whole. Configurable
admission settings and global memory/response-size controls are separate work.
