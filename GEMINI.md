# Bolt (⚡) - Project Context

## Project Overview
Bolt is a **high-performance**, **Windows-first** HTTP/1.1 static file server written in **C**. It is designed for maximum throughput and scalability on Windows systems by leveraging native asynchronous I/O mechanisms.

**Key Technologies:**
*   **Windows IOCP (I/O Completion Ports):** For scalable event-driven I/O.
*   **AcceptEx:** For high-performance connection acceptance.
*   **TransmitFile:** For zero-copy file transfers directly from the kernel cache.
*   **Thread Pool:** Worker threads processing IOCP events.
*   **Memory Pool:** Custom allocator to reduce heap fragmentation.
*   **MinGW / w64devkit:** The required build toolchain.

## Building and Running

### Prerequisites
*   Windows OS
*   MinGW toolchain (specifically `w64devkit` is recommended)
*   `make`

### Build Commands
*   **Default Build:** `make` (Compiles with `-O3` and standard flags)
*   **Release Build:** `make release` (Adds `-flto`, `-DNDEBUG`, and maximum optimizations)
*   **Debug Build:** `make debug` (Adds `-g`, `-O0`, `-DBOLT_DEBUG`)
*   **Clean:** `make clean`

### Running the Server
*   **Start Server:** `./bolt.exe` (Default port: 8080)
*   **Custom Port:** `./bolt.exe 3000`
*   **With Stats:** `./bolt.exe 8080 --stats --stats-interval-ms 1000`

### Testing
*   **Run All Tests:** `make test` (Compiles and runs `test_runner.exe`)
*   **Run Unit Tests Only:** `make test-unit`
*   **Test Framework:** Uses `minunit.h` for unit and integration testing.

## Development Conventions

### Directory Structure
*   `src/`: Implementation files (`.c`)
*   `include/`: Header files (`.h`)
*   `public/`: Default root directory for served files (contains `index.html`)
*   `tests/`: Unit and integration test source files
*   `bench/`: Benchmark documentation
*   `obj/`: Compiled object files

### Coding Style & Patterns
*   **Language:** C (C99/C11 standard implied)
*   **Naming:** Snake_case for functions and variables (`bolt_server_init`, `client_socket`).
*   **Prefixes:** Public API functions and types often use `bolt_` or `Bolt` prefix (e.g., `BoltServer`, `bolt_server_start`).
*   **Error Handling:** Return codes (usually 0 for success, non-zero for error) and logging via `BOLT_ERROR` macro.
*   **Configuration:** Compile-time configuration knobs are located in `include/bolt.h` (e.g., buffer sizes, thread counts, timeouts).
*   **Testing:** New features should include corresponding tests in the `tests/` directory, registered in `tests/test_main.c`.

### Key Files
*   `include/bolt.h`: Main configuration header and architectural definitions.
*   `src/bolt_server.c`: Core server logic and initialization.
*   `src/iocp.c`: Wrapper around Windows IOCP functions.
*   `src/http.c`: HTTP request parsing and response formatting.
*   `Makefile`: Build definitions.
