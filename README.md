# Bolt (⚡) — High-Performance HTTP Static File Server (Windows)

Bolt is a **high-performance**, **Windows-first** HTTP/1.1 static file server written in C. It is built around Windows IOCP for scalable async I/O and uses kernel-assisted file transfer for throughput.

**This project was created with AI assistance** (Cursor + GPT-5.2). The code and docs were generated and iterated collaboratively; you should review it before using it in production.

## Features

- **Windows IOCP**: scalable async network I/O.
- **AcceptEx**: pre-posted accepts with optional “receive-on-accept”.
- **Keep-Alive**: connection reuse to reduce handshake overhead.
- **TransmitFile (zero-copy)**: kernel-assisted file transfer for large assets.
- **Small-file in-memory cache**: accelerates mixed websites (HTML+CSS+JS+small images).
- **Path sanitization**: blocks traversal attempts like `..`.
- **MIME detection**: common web asset types.
- **Optional directory listing**: disabled by default for maximum performance.

## Project Layout

```
.
├── include/              # Public headers
├── src/                  # Implementation
├── public/               # Website root (served files)
├── bench/                # Benchmark notes
├── Makefile              # MinGW/w64devkit build
└── .gitignore
```

## Build

Requires a MinGW toolchain (e.g., `w64devkit`) on Windows.

```powershell
make
```

For maximum optimization:

```powershell
make release
```

## Run

```powershell
./bolt.exe
```

Custom port:

```powershell
./bolt.exe 3000
```

Enable periodic stats printing:

```powershell
./bolt.exe 8080 --stats --stats-interval-ms 1000
```

Then open:
- `http://localhost:8080/`
- `http://localhost:8080/about.html`

## Design Decisions (and why)

### IOCP-first networking
**Goal:** handle many concurrent connections efficiently without a thread-per-connection model.

- All socket activity is driven by **I/O Completion Port** events.
- A worker **thread pool** waits on the IOCP queue and processes completions.

### AcceptEx + receive-on-accept
**Goal:** reduce syscalls and latency on new connections.

- We pre-post accepts so the kernel can accept connections immediately.
- We optionally receive a small amount of initial request bytes during accept to reduce an extra recv.

### Keep-Alive default
**Goal:** mixed-site performance.

Browsers request many assets (HTML, CSS, JS, images). Reusing connections drastically reduces overhead and improves latency.

### TransmitFile for large assets
**Goal:** maximum throughput with minimal CPU.

For non-trivial file sizes, we use `TransmitFile()` so the kernel can move file data to the network stack efficiently (reduced user-space copies).

### Small-file cache for mixed-site speed
**Goal:** reduce disk + open/close overhead for hot small assets.

For files under a configured threshold, Bolt caches `{headers, body}` in memory (validated by `mtime` + size) and responds with a single async send.

### Directory listing disabled by default
**Goal:** “fastest server” focus.

Directory listing is expensive and not needed for typical website serving, so it is disabled unless you explicitly enable it.

## Configuration Knobs

Most performance knobs are in:
- `include/bolt.h`

Key toggles:
- `BOLT_ENABLE_DIR_LISTING` (default `0`)
- `BOLT_ENABLE_FILE_CACHE` (default `1`)
- `BOLT_FILE_CACHE_MAX_ENTRY_SIZE`, `BOLT_FILE_CACHE_MAX_TOTAL_BYTES`
- `BOLT_THREADS_PER_CORE`
- `BOLT_ACCEPT_RECV_BYTES`

## Benchmarks

See:
- `bench/README.md`

To make “fastest” claims meaningful, benchmarks must specify:
- hardware + OS
- workload definition (mixed site vs small file vs large throughput)
- exact tool + command line
- configuration flags

## Safety Notes / Limitations

- This is an educational/experimental performance-focused server.
- TLS/HTTPS is **not** implemented.
- HTTP parsing is intentionally minimal (GET/HEAD oriented).
- Always treat untrusted input carefully; review security before exposing to the internet.

## License

Add a license before publishing if you want reuse/attribution terms to be explicit (MIT is a common choice).


