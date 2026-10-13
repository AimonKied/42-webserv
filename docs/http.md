# HTTP Reference

This document will describe the HTTP request and response flow for webserv.

## Topics

- Request parsing
- Response formatting
- Common status codes

## Request size limits

`client_max_body_size` is a per-server limit on decoded body bytes. Headers,
request-line bytes, chunk-size lines, extensions and chunk separators do not count
towards that body limit. Zero allows an empty body. A Content-Length declaration
or a chunk that would exceed the body limit produces 413 without waiting for the
remaining body. A body exactly at the limit is allowed.

The old fixed one-MiB raw-buffer cap has been removed. The parser owns the limits
and exposes `HttpParser::maxBufferedRequestSize(limit)` for the networking layer's
pre-parse guard. It allows the configured body plus the existing request-line and
header limits, and a separate chunk-framing allowance:

| Component | Limit |
| --- | --- |
| Decoded body | Configured `client_max_body_size` |
| Request line | 8192 bytes (414 on overflow) |
| Header block | Existing 16384-byte parser limit (431 on overflow) |
| Each chunk-size/extension line | 8192 bytes (413 on overflow) |
| Total chunk framing | `5 * bodyLimit + 65536` bytes (413 on overflow) |

Five framing bytes per body byte accommodate even one-byte chunks (`1\r\nx\r\n`).
Larger normal chunks need less framing per byte. The additional 64 KiB covers the
final chunk and extensions/padding. Excessive metadata can therefore be rejected
independently of an otherwise acceptable decoded body. Trailers remain unsupported.

The raw guard adds all these allowances, including request/header terminators,
using saturating arithmetic so a large configured limit cannot wrap to a small
buffer budget. The guard runs after each receive; the raw buffer can exceed its
budget by at most one receive before rejection. Buffering is still whole-request,
so operators must choose body limits appropriate to available memory.

Boundary tests:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/request_limits.cpp src/HttpParser.cpp -o /tmp/webserv-request-limits-test
/tmp/webserv-request-limits-test
python3 tests/body_limits_integration.py ./webserv
```

These cover exact/below/above limits, zero-byte limits, limits above one MiB,
fragmented bodies, chunk framing, independent listeners, metadata limits and
overflow-safe budget calculations.
