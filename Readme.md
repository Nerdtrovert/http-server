# Production-Quality I/O Multiplexed HTTP Server in C

A modular, highly-optimized HTTP/1.1 server built for Linux/POSIX systems. Designed from first principles to explore low-level network programming, kernel system calls, memory safety, and concurrent connection handling.

---

## 🎯 Architectural Mindset & Goals

This project serves as a hands-on systems programming curriculum. Rather than relying on high-level libraries or frameworks, we interface directly with the Linux kernel via POSIX system calls.

* **Deep Understanding Over Speed**: We construct socket listeners, routing engines, child process lifecycles, and parser state machines from scratch to understand the mechanics of the kernel network stack.
* **Defensive C Engineering**: We apply strict validation, memory isolation, bounds-checking, and compiler diagnostics to achieve production-grade reliability and security.
* **Empirical Diagnostics**: We leverage dynamic analysis tools (ASan, Valgrind, strace, perf) to profile bottlenecks, trace kernel-user transitions, and guarantee leak-free execution.

---

## 🏗️ Concurrency Architecture (I/O Multiplexing Flow)

Under production loads, I/O multiplexing with `select()` or `poll()` allows handling multiple concurrent connections within a single process, eliminating process creation overhead while efficiently managing file descriptors.

```text
                          Single Process
                                 │
                     server_accept()  <── [Handles EINTR by retrying]
                                 │
                      Add new FD to fd_set/array
                                 │
                          select()/poll() call
                                 │
                        Ready FDs detected
                                 │
          ┌──────────────────────┼──────────────────────┐
          │                      │                      │
   New connection       Data to read       Data to write
          │                      │                      │
   Accept client    Read request   Write response
          │                      │                      │
   Add to set       Process req    Send data
          │                      │                      │
   Monitor for    Check for      Monitor for
   readability    completion     writability
          │                      │                      │
  ┌─────────┐   └──────────┐   ┌────────────┐
   │         │              │  │            │
Process   Close if     Remove     Close if
client    done/error    from set   done/error
          │                      │                      │
   Continue loop          Continue loop        Continue loop
```

---

## 🗺️ Learning Roadmap & Evolution

The codebase undergoes nine sequential stages of evolution. Each version introduces specific kernel APIs, architectures, and performance challenges:

### V1: Blocking Single-Client TCP Server
* **APIs**: `socket()`, `bind()`, `listen()`, `accept()`, `read()`, `write()`
* **Details**: Building a single blocking listener, establishing 3-way handshakes, and returning static responses. Focuses on core socket life-cycle, binding ports, backlog sizing, and handling basic read/write errors.

### V2: Routing & HTTP Parsing State Machines
* **APIs**: Strict buffer traversal pointers (no mutating `strtok`)
* **Details**: Modular parsing of HTTP requests, path verification, and MIME-type classification. Explores safe buffer handling, prevents path traversal attacks, and routes static files securely.

### V3: Multi-Process Concurrency
* **APIs**: `fork()`, `signal(SIGCHLD)`, `waitpid()`, `errno == EINTR`
* **Details**: Handling multiple concurrent clients via process cloning. Closes unnecessary descriptors in parent/child scopes, intercepts `SIGCHLD`, and reaps zombie processes asynchronously via `WNOHANG`.

### V4: I/O Multiplexing with `select()`
* **APIs**: `select()`, `fd_set`, `FD_SET`, `FD_CLR`
* **Details**: Handling concurrent sockets inside a single process, avoiding process creation overheads, and analyzing the `FD_SETSIZE` ceiling and bitmask limitations.

### V5: I/O Multiplexing with `poll()` (Current version)
* **APIs**: `poll()`, `struct pollfd`, `POLLIN`, `POLLOUT`
* **Details**: Using dynamically allocated/sized arrays of descriptors, separating input events from output revents, and implementing $O(1)$ connection array compaction to avoid linear shift overheads.

### V6: Event-Driven Engine via `epoll()`
* **APIs**: `epoll_create1()`, `epoll_ctl()`, `epoll_wait()`, non-blocking I/O (`O_NONBLOCK`)
* **Details**: Harnessing Linux-specific event queues. Explores Level-Triggered (LT) vs Edge-Triggered (ET) notification modes and implements non-blocking read/write loops to handle massive traffic spikes.

### V7: Multi-Threaded Concurrency
* **APIs**: `pthread_create()`, `pthread_mutex_t`, `pthread_cond_t`
* **Details**: Building thread-safe shared work queues and worker thread pools. Avoids the overhead of process creation (`fork`), focuses on synchronization primitives, and resolves potential race conditions.

### V8: Zero-Copy Optimizations
* **APIs**: `sendfile()`, `mmap()`, `munmap()`
* **Details**: Optimizing file reads by bypassing user-space buffer copies for high-throughput I/O. Reduces context switches and offloads paging and disk-to-socket transfers directly to the Linux page cache.

### V9: Daemonization & Hardening
* **APIs**: `sigaction()`, `fork()`, `setsid()`, `syslog()`, `chroot()`
* **Details**: Proper daemonization process lifecycle, syslog integration, signal safety, and clean signal-based shutdown. Ensures the server runs gracefully as a persistent background daemon.

---

## ⚙️ Core Modules & API Specification

The implementation is divided into clean, isolated modules to enforce strong separation of concerns:

### Socket Listener Subsystem (`include/server.h`)
* `Server *server_create(int port)`: Allocates and initializes the server structure, binds a socket, sets `SO_REUSEADDR`, and triggers listener mode.
* `int server_accept(Server *server)`: Wrapper around POSIX `accept` that blocks to pull a connection off the kernel queue. Retries accept loop when interrupted by `EINTR`.
* `void server_close(Server *server)`: Tears down memory and closes all open socket descriptors.
* `void server_close_listener(Server *server)`: Disconnects the listening descriptor while leaving open child client connections unaffected.

### File Utility Module (`include/file.h`)
* `int build_file_path(const char *request_path, char *file_path, size_t file_path_size)`: Resolves safety/bounds and maps paths to `www/`. Prevents `/../` directory traversal.
* `const char *get_mime_type(const char *file_path)`: Resolves file extensions to MIME types (`text/html`, `text/css`, `image/png`, etc.).

### Protocol Processing Subsystem (`include/http.h`)
* `int http_handle(int client_fd)`: Handles client request parsing, routes requested paths, serves responses, and closes connection descriptors.
* `int send_response_header(int client_fd, int status_code, const char *status_text, const char *content_type, size_t content_length)`: Composes and sends a standardized HTTP header block.

---

## 🛡️ Systems-Level Safety & Parsing Rules

Web servers are primary attack vectors. We implement strict defenses against typical C vulnerabilities:

### 1. Parsing Vulnerabilities & Fixes
* **Index Desync Prevention**: Standard token parsers can easily fail to advance pointers when input exceeds buffer bounds. We enforce explicit check limits and error out immediately if a token exceeds bounds, preventing downstream index corruption.
* **Strict Delimiter Assertions**: Instead of permissive loops matching whitespace, we enforce strict single-space tokens (`%20` parsing and standard `SP`). Deviations from the protocol specification (e.g. consecutive spaces) are rejected to prevent HTTP Request Smuggling.
* **Line Terminator Validation**: We explicitly enforce carriage return and line feed checking (`\r\n`) at the end of request streams to protect against header injection attacks.

### 2. Socket and Memory Security
* **Zero-Copy Buffer Slicing**: Rather than mutating inputs using `strtok()`, our HTTP parser uses absolute pointer offsets and length boundaries to mitigate null-byte injection attacks.
* **Fractional Send Loop**: TCP does not guarantee complete payload delivery in a single `write()` call. We implement `send_all()` loops to retry partial socket transmissions, checking socket states via loop iterations.
* **FD Resource Management**: We track all file descriptors in our fd_set/array and ensure proper cleanup when connections close to prevent resource leaks.

---

## 📁 Directory Structure

```text
.
├── Makefile              # Build automation configurations
├── Readme.md             # Architecture and curriculum overview
├── include/              # Public module header files
│   ├── file.h            # Path routing and MIME utility declarations
│   ├── http.h            # State-machine parsing & handler declarations
│   └── server.h          # TCP connection and socket manager
├── src/                  # Implementation files
│   ├── file.c            # File routing and MIME classification
│   ├── main.c            # Application entrypoint & I/O multiplexing logic
│   ├── server.c          # Listener configuration and accept loop
│   └── http.c            # HTTP protocol and response utilities
└── www/                  # Static HTML assets served by the engine
```

---

## ⚙️ Build Automation (Makefile)

For reproducible compilation across environments, the following `Makefile` manages compiler flags, output files, directories, and diagnostics:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = mini_http

all: CFLAGS += -O2
all: $(TARGET)

debug: CFLAGS += -g -fsanitize=address,undefined
debug: TARGET = mini_http_asan
debug: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f src/*.o mini_http mini_http_asan
```

---

## 🛠️ Build, Profile & Diagnostics Guide

The build system requires modern GCC and POSIX environments.

### Compilation Targets

```bash
# Production/Release Build (High optimizations)
gcc -O2 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o mini_http

# Debug Build with Sanitizers (Address & Undefined Behavior Sanitizers)
gcc -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -Iinclude src/*.c -o mini_http_asan
```

### Dynamic Analysis & Instrumentation

* **Valgrind Runtime Leak Checks**:
  `valgrind --leak-check=full --show-leak-kinds=all ./mini_http`
  Used to identify active memory leaks, dangling pointer references, double-frees, or usage of uninitialized stack values.
* **System Call Tracing (`strace`)**:
  `strace -f -e trace=network,desc,process ./mini_http`
  Intercepts and records network and descriptor system calls. Allows real-time analysis of `select()`, `poll()`, `accept()`, `recv()`, `send()`, `sendfile()`, and socket lifecycles.
* **CPU Profiling (`perf`)**:
  `perf record -g ./mini_http`
  Profiles hot paths, function runtime footprints, and performance bottlenecks inside user space and kernel space context switches.

---

## 📥 Sample Input & Verification

Simulate client workloads using `netcat` (`nc`) or `curl` to test server responses:

### 1. Retrieve Root Resource (GET)
```bash
# Input Request
printf "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc 127.0.0.1 8080

# Expected Response
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 124

<!DOCTYPE html>
<html>...
```

### 2. Retrieve Headers Only (HEAD)
```bash
# Input Request
printf "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc 127.0.0.1 8080

# Expected Response (No body sent)
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 124
```

### 3. Malformed Method Check (Strict Rejection)
```bash
# Input Request
printf "POST / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc 127.0.0.1 8080

# Expected Response
HTTP/1.1 405 Method Not Allowed
```

### 4. Delimiter Validation Failure
```bash
# Input Request (Multiple spaces)
printf "GET  /  HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc 127.0.0.1 8080

# Expected Response
HTTP/1.1 400 Bad Request
```

### 5. Nested Directory Traversal Attempt
```bash
# Input Request (Directory traversal prevention check)
printf "GET /../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc 127.0.0.1 8080

# Expected Response
HTTP/1.1 404 Not Found
```

---
