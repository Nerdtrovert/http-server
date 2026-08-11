# Production-Quality Mini HTTP Server in C

Welcome to the **Mini HTTP Server** project! This repository contains a production-quality, modular, and progressively evolving HTTP server written in C for Linux/POSIX systems. Designed as a long-term hands-on systems programming learning curriculum, this project moves step-by-step from a basic blocking single-client socket server up to a highly-optimized, zero-copy, event-driven socket engine.

---

## 🎯 Project Mindset & Goals
This project is built under the guidance of a senior Linux Systems Engineer. The core goals are:
* **Deep Understanding Over Speed**: Build core components from first principles. Understand *why* APIs exist, *how* the Linux kernel handles them under the hood, and *where* production systems use them.
* **Production-Quality C**: Emphasize modular design, strict separation of concerns, clean memory/resource management, precise error handling, robust input validation, and proper POSIX interfaces.
* **Empirical Debugging**: Leverage industry-standard tooling (`gdb`, `strace`, `valgrind`, sanitizers like ASan/UBSan, and `perf`) to diagnose issues and optimize performance.

---

## 🗺️ Learning Roadmap & Evolution

The server will evolve through the following stages, where each version refactors and builds upon the previous one. We do not skip versions.

| Version | Architecture Type | Key Features / Linux APIs |
| :--- | :--- | :--- |
| **Version 1** | **Blocking / Single-Client** | TCP Sockets (`socket`, `bind`, `listen`, `accept`), Static HTML response |
| **Version 2** | **File Routing & Parsing** | Robust request-line parsing, file mapping, MIME-type classification |
| **Version 3** | **I/O Multiplexing (select)** | Handling multiple concurrent clients using `select()` |
| **Version 4** | **I/O Multiplexing (poll)** | Transitioning to `poll()`, discussing FD limits and kernel overhead |
| **Version 5** | **Event-Driven (epoll)** | Level-triggered vs Edge-triggered `epoll()`, non-blocking socket handling |
| **Version 6** | **Concurrent Thread Pool** | Work queues, Worker threads, Thread pools, Mutexes, Condition Variables |
| **Version 7** | **Zero-Copy Optimizations** | High-performance content delivery via `sendfile()` and `mmap()` |
| **Version 8** | **Production Hardening** | Configuration parser, robust syslog/logging, graceful shutdown (`sigaction`) |

---

## 📁 Directory Structure

```text
.
├── Makefile              # Build automation config
├── Readme.md             # Project roadmap and architecture documentation (this file)
├── interview.md          # Technical summaries and interview discussion points
├── include/              # Public module header files
│   ├── http.h            # HTTP request parsing and response handlers
│   └── server.h          # TCP connection and socket listener management
├── src/                  # Source implementation files
│   ├── main.c            # Application entry point & high-level coordinator
│   ├── server.c          # Socket listening, binding, and accepting logic
│   └── http.c            # HTTP protocol logic, parsing, and I/O handlers
├── www/                  # Static assets served by the server
└── test/                 # Test scripts and client simulators
```

---

## 🛠️ Build and Debug Guide

### Compilation

You can compile the server using the following commands:

* **Production/Release Build** (Optimized, no sanitizers):
  ```bash
  gcc -Wall -Wextra -Wpedantic -Iinclude \
      src/main.c src/server.c src/http.c \
      -o mini_http
  ```

* **Debug Build with Sanitizers** (AddressSanitizer and UndefinedBehaviorSanitizer enabled):
  ```bash
  gcc -Wall -Wextra -Wpedantic \
      -fsanitize=address,undefined \
      -g \
      -Iinclude \
      src/main.c src/server.c src/http.c \
      -o mini_http_asan
  ```

### Run and Debug Commands
```bash
# Run the standard build
./mini_http

# Run the sanitized debug build (recommended during development)
./mini_http_asan

# Run memory checks under Valgrind (do not combine with ASan)
valgrind --leak-check=full --show-leak-kinds=all ./mini_http

# Monitor system calls during execution
strace -f ./mini_http
```

---

## 🧑‍🏫 Mentorship Rules of Engagement
For every new module or feature version:
1. **Concept First**: We explore *Why*, *What*, *How*, and *Where* before writing a single line of code.
2. **Modular Architecture**: No monolithic `main.c`. High cohesion, low coupling.
3. **Strict Step Execution**:
   - Discuss architecture and APIs.
   - Design module responsibilities and interfaces.
   - Trace execution flow.
   - Implement one single module at a time.
   - Verify, review code, address sanitizers, and debug.
   - Proceed only upon explicit student approval.
