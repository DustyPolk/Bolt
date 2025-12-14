# Bolt Bench (mixed-site)

This directory documents a repeatable way to measure Bolt’s **mixed-asset website** performance (HTML+CSS+JS+images) on Windows.

## Build (release)

```powershell
make release
```

## Run with stats

```powershell
./bolt.exe 8080 --stats --stats-interval-ms 1000
```

## Recommended load tools (Windows-friendly)

- **wrk** (if you have it): use multiple threads and connections to simulate browsers.\n
- **hey** (Go tool): simple and easy.\n
- **curl**: sanity checks only (not a benchmark).

## Example: mixed-site smoke test

Open these in a browser (should be instant after warmup):

- `http://localhost:8080/`\n
- `http://localhost:8080/about.html`\n

## Example: concurrency test

Use your preferred tool to hit the home page and a couple static assets repeatedly.\n
For a mixed-site test, include a representative JS/CSS/image path from your `public/` directory.

## Notes on “fastest”

To legitimately claim “fastest”, you need:\n
- a published benchmark harness\n
- identical hardware + OS\n
- comparable features (keep-alive, caching)\n
- results across multiple workloads (small file, mixed site, large file)


