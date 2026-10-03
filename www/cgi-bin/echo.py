#!/usr/bin/python3
"""Buffered CGI demo: stdin is the decoded request body; stdout is CGI output."""
import json
import os
import sys

body = sys.stdin.buffer.read()
print('Content-Type: application/json')
print()
print(json.dumps({
    'method': os.environ.get('REQUEST_METHOD'),
    'query': os.environ.get('QUERY_STRING'),
    'port': os.environ.get('SERVER_PORT'),
    'body': body.decode('utf-8', errors='replace'),
    'body_bytes': len(body),
}))
