1. Basic success
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 200 OK
Content-Type: text/html
...
2. 404 — unknown path
printf 'GET /abc HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 404 Not Found
3. 404 — nested path
printf 'GET /hello/world HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 404 Not Found
4. 405 — unsupported method
printf 'POST / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 405 Method Not Allowed
5. Another unsupported method
printf 'PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 405 Method Not Allowed
6. Unsupported HTTP version
printf 'GET / HTTP/1.0\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 400 Bad Request
7. Malformed request line
printf 'GET / HTTP\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 400 Bad Request
8. Missing path
printf 'GET  HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 400 Bad Request
9. Multiple spaces

According to our V1 strict parser, this should fail:

printf 'GET  / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 400 Bad Request
10. Short/malformed version
printf 'GET / H\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 400 Bad Request
11. Empty request
printf '\r\n\r\n' | nc 127.0.0.1 8080

Expected:

HTTP/1.1 400 Bad Request