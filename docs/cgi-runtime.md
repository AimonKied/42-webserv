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

Run the integration suite with `python3 tests/cgi_integration.py ./webserv`.
It creates temporary config/scripts, selects two free local ports, and stops its
own server. Python at `/usr/bin/python3` is required. Linux additionally checks
child reaping, descriptor leaks and socket inheritance through `/proc`.

## Ownership and flow

1. `Server::handleClientRead` passes accumulated bytes to Joey's `HttpParser`.
   Only a complete, valid request can reach CGI dispatch. Chunked input is decoded
   by the parser before execution.
2. `ServerCgi.cpp` is a temporary routing adapter. It calls Simon's
   `resolveCgiTarget` and `buildCgiEnv`, validates the script's path under the
   configured root, and prepares a `CgiLaunch` value.
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

`CgiExecutor` owns child PIDs and pipe descriptors. `Server` owns listeners and
client sockets. `cleanupClient` cancels the corresponding job before closing the
socket. Cancelled jobs are keyed by PID and disassociated from the client fd until
reaped; fd reuse cannot deliver an old result to a new client. Shutdown kills all
active children, closes their pipes, and reaps them before closing sockets.
Normal event-loop reaping never blocks; teardown waits after sending SIGKILL.

## Explicit temporary decisions

- One root (`/`) location per configured server, enforced at startup. Multiple
  server blocks/listeners are supported; listener fd maps to configuration index.
- Python only, interpreter `/usr/bin/python3`. Other configured CGI extensions
  return 501 rather than falling through to static serving.
- `ServerCgi.cpp` bridges the unfinished response-or-CGI routing handover. It
  checks allowed methods and redirects for CGI requests. Simon should eventually
  supply the matched location, validated script path and launch instruction from
  his routing layer. This does not implement general location selection.
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
cancellation; fd/child cleanup; shutdown during an active CGI.

Simon's CGI-output parser remains the HTTP boundary. Its broader protocol
hardening (for example hop-by-hop response headers, strict Status validation and
repeated headers) is separate from this executor implementation.
