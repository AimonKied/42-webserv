# Overload and stress tests

Build from the repository root, then run:

```sh
make
python3 tests/overload_integration.py ./webserv
```

The test creates its own temporary root/configuration, starts one server on two
available loopback ports, and stops it afterwards. It never sends requests to an
existing server. Python at `/usr/bin/python3` is needed for the CGI fixtures.

The admission checks use the defaults from `RuntimeLimits.hpp` (128 clients,
16 CGI jobs). Update the test constants if those defaults change.

Checks:

- Fill all CGI slots, observe 503 with Retry-After for additional CGI requests,
  and verify static responses on both listeners still work.
- Recover capacity after successful completion, client cancellation and timeout.
  On Linux, verify child counts and descriptor counts return to baseline.
- Fill all client slots. New connections remain pending while an existing client
  can still complete. Verify accepting resumes when that client frees a slot.
- Send 600 mixed static, CGI, missing-file and 256-KiB file requests with 32 workers.
  Verify bodies/status codes and cleanup, allowing deliberate CGI 503 responses.
- On Linux, start a separate server with RLIMIT_NOFILE=32 and exhaust its actual
  descriptor table. Verify accept retries are rate-limited, a CGI pipe failure
  yields 503, and service recovers after descriptors are released.

For a larger bounded run:

```sh
python3 tests/overload_integration.py ./webserv --requests 3000 --workers 64
```

This is a functional stress check, not a throughput benchmark or proof against
all resource exhaustion. Whole-response buffering and aggregate memory budgets
remain separate concerns. Worker counts exceeding the client cap can also fill
the OS backlog; connection timeouts/refusals in that scenario are intentional
backpressure, not HTTP 503 responses.

## Deterministic launch failures (Linux / GNU linker)

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/cgi_launch_failures.cpp src/CgiExecutor.cpp -Wl,--wrap=pipe -Wl,--wrap=fork -o /tmp/webserv-cgi-launch-failures
/tmp/webserv-cgi-launch-failures
```

Only this test binary wraps pipe/fork. It forces first-pipe, second-pipe and fork
failures, repeats them 100 times, and checks result classification plus descriptor
and job cleanup. No process-table exhaustion or real child launch is needed.
The production binary is not instrumented.

Run the existing body-limit, socket, routing and CGI suites alongside these tests
when changing admission/lifecycle behavior.
