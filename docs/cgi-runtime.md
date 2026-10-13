# CGI runtime and team handover

The executor is connected to the server's existing single poll loop. Build and
run from the repository root:

```sh
make
./webserv config/cgi.conf
```

In another terminal:

```sh
curl 'http://127.0.0.1:8080/cgi-bin/echo.py?hello=world'
curl --data 'hello from POST' http://127.0.0.1:8081/cgi-bin/echo.py
```

Run the executor suite with `python3 tests/cgi_integration.py ./webserv` and the
multi-location suite with `python3 tests/routing_integration.py ./webserv`.
It creates temporary config/scripts, selects two free local ports, and stops its
own server. Python at `/usr/bin/python3` is required. Linux additionally checks
child reaping, descriptor leaks and socket inheritance through `/proc`.

## Ownership and flow

1. `Server::handleClientRead` passes accumulated bytes to Joey's `HttpParser`.
   Only a complete, valid request can reach CGI dispatch. Chunked input is decoded
   by the parser before execution.
2. `handleRequest` in `RequestHandler.cpp` selects the longest matching location
   using Simon's `findBestLocation`. Request errors, method restrictions (including
   the Allow header) and configured redirects take precedence over CGI execution.
   The handler returns `RequestOutcome`: a `std::variant<Response, CgiLaunch>`
   plus the matched location pointer. It calls `resolveCgiTarget` and `buildCgiEnv`
   for CGI and validates the script path under the matched root. A missing location
   produces a ready error response; there is no first-location fallback.
   `Server::handleClientRead` inspects the outcome: it queues a Response or calls
   `_cgi.start` with the CgiLaunch. The launch contract lives in `CgiLaunch.hpp`,
   independently of executor internals.
3. `CgiExecutor::start` creates pipes, forks and execs the interpreter. Child stdin
   and stdout are blocking; the parent pipe endpoints are nonblocking. Server
   sockets and pipe descriptors have close-on-exec set. The child changes to the
   script directory and uses an explicit environment, without invoking a shell.
4. The client enters `WaitingForCgi`. The executor contributes pipe descriptors to
   the same readiness snapshot as listeners and connected sockets. The only
   `poll()` call remains in `Server::run`. One read/write per ready pipe per turn
   allows socket work and other CGI jobs to progress.
5. Input is written with an offset and closed when exhausted, delivering EOF.
   Output is buffered up to its cap. `POLLHUP` on stdout triggers draining rather
   than discarding remaining bytes. `waitpid(..., WNOHANG)` runs every active turn.
6. Successful completion needs both child reaping and stdout EOF. The collected
   bytes go to Simon's `buildCgiResponse`, then `queueResponse` selects POLLOUT for
   the client socket. The existing partial-send path transmits the response.
   `Client::cgiLocation` preserves the matched location through execution, output
   parsing and error construction. It borrows from Server's const configuration,
   which outlives clients, and is cleared when the response is queued. Configuration
   reload would require revisiting that lifetime contract.

`CgiExecutor` owns child PIDs and pipe descriptors. `Server` owns listeners and
client sockets. `cleanupClient` cancels the corresponding job before closing the
socket. Cancelled jobs are keyed by PID and disassociated from the client fd until
reaped; fd reuse cannot deliver an old result to a new client. Shutdown kills all
active children, closes their pipes, and reaps them before closing sockets.
Normal event-loop reaping never blocks; teardown waits after sending SIGKILL.

## Explicit temporary decisions

- Multiple locations and server blocks/listeners are supported; listener fd maps
  to configuration index. Both static and CGI decisions use the longest matching
  location. URL-to-filesystem mapping retains the existing root + full request
  path convention; location-prefix stripping is a separate follow-up.
- Python only, interpreter `/usr/bin/python3`. Other configured CGI extensions
  return 501 rather than falling through to static serving.
- `RequestHandler.cpp` is the routing handover for Simon to extend; it neither
  launches processes nor touches client sockets. `ServerCgi.cpp` only queues
  responses and converts completed executor results into HTTP responses.
- Simon's existing matcher supports an extension at the end of the path, not
  `/script.py/additional/path`. PATH_INFO remains empty under that matcher.
- Five-second wall-time limit measured with a steady clock; four-MiB buffered
  output limit. These constants live in `CgiExecutor.cpp`, pending config fields.
- No streaming: the full CGI output is validated before an HTTP response is sent.
- Nonzero child exit, launch/pipe failure and output overflow become 502; deadline
  expiration becomes 504. Missing scripts return 404, root escapes 403.
- Client EOF while waiting cancels the job. TCP write-half-close support and
  persistent/pipelined requests are not implemented.
- The executor manages the directly launched child. Process-group management for
  scripts that spawn long-lived descendants is a later extension. A descendant
  holding stdout open cannot make the client wait beyond the CGI deadline.
- The pre-existing one-MiB raw request-buffer cap remains distinct from configured
  body size. It now produces a 413 response, but reconciling raw framing overhead
  with the body limit remains outstanding.

## Tests covered

Two listeners and static files; CGI environment and cwd; fragmented Content-Length
and chunked POST; simultaneous input/output larger than pipe capacity; draining
stdout after the direct child exits; malformed output; nonzero child exit; output
cap; missing scripts; symlink escape rejection; socket inheritance; timeout with
static service on the other listener; EOF before child exit; repeated client
cancellation; fd/child cleanup; shutdown during an active CGI. Multi-location
coverage includes reversed config order, dedicated CGI routes, method restrictions,
separate roots, static and CGI redirects, no-match 404s, unsupported interpreters,
and location-specific CGI 502/504 pages.

Simon's CGI-output parser remains the HTTP boundary. Its broader protocol
hardening (for example hop-by-hop response headers, strict Status validation and
repeated headers) is separate from this executor implementation.
