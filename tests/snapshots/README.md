# Snapshot regression tests

The public-libretro host comes from a pinned RetroArch revision.
No Beetle checkout or core-private frontend code is required. Build the core,
then run:

```sh
make -C tests/snapshots check CORE=/path/to/swanstation_libretro.so \
  SYSTEM=/read-only/firmware/directory OUTPUT=/new/writable/path
make -C tests/snapshots check-gpu CORE=/path/to/swanstation_libretro.so \
  FRONTEND=/path/to/retroarch SYSTEM=/read-only/firmware/directory \
  OUTPUT=/another/new/path
make -C tests/snapshots check-pgxp
python3 tests/snapshots/check_pgxp_mutants.py --output /new/mutant/path
```

Python 3 runs profiles; C and C++17 compilers build tests. Native Windows uses
MinGW and `SHELL=C:/msys64/usr/bin/sh.exe`; tool arguments use Windows paths
with forward slashes. Firmware is not distributed or modified. The generated
original PS-X EXE needs a compatible BIOS for this short startup window;
the built-in BIOS did not reach this workload within that window.

The five software profiles test frontend-owned prediction, accepted but
inactive registration, declined registration and native internal run-ahead.
Internal depth two remains active with an ordinary/declined frontend; active
frontend snapshots take ownership and match the internal-depth-zero result.
Guest reads, RAM and software RGB are compared over 32 continuations with
depths 1/2/3/12, repeated loads and independently released owners. Rejected
ordinary loads must preserve execution and recovery captures; successful
ordinary loads invalidate those captures.

The four GPU profiles cover OpenGL/Vulkan 4x, with PGXP off and with PGXP plus
its vertex cache enabled. They compare guest RAM and decoded final frontend
RGB with uninterrupted references using the same driver/settings. The proxy
schedules snapshots itself, separately from frontend run-ahead integration.

The PGXP test uses the shipping implementation and actual memory/register/
vertex consumers. Native packed coordinates deliberately stay equal while
fractional positions change. It checks shared zero/unchanged pages, repeated
restores, independent ownership, trimming and eight reached allocation-failure
sites after successful capture. Four omitted-restore mutants must fail the
consumer comparisons; the vertex-cache mutant must pass with that cache off.

The software/GPU suites and 133 PGXP assertions pass on Windows x86_64/NVIDIA.
The new GPU tests are not performance tests. Linux/Android execution, D3D,
physical device loss, allocation failure inside GPU state commits, media and
all setting transitions remain untested here. Do not treat these finite
fixtures as universal determinism or original-console accuracy proof.

## Test harness dependency

Only these explicit test targets fetch sources from RetroArch. Normal core
builds and gameplay do not download or use the test harness. `harness.lock.json`
pins a full commit and SHA-256 for each source file, including the matching
libretro header. The first `make harness`, `check` or `check-gpu` downloads
the sources into a shared user cache; later runs verify and reuse them.
The cache is under `%LOCALAPPDATA%/libretro/snapshot-tests` on Windows, or
`$XDG_CACHE_HOME/libretro/snapshot-tests` (`~/.cache` by default) elsewhere. Only targets supported by this core are available.

Use `OFFLINE=1` to require cached sources, or set
`TOOLS=/path/to/RetroArch/libretro-common/samples/snapshots` for local
development or an offline source checkout. `HARNESS_CACHE` selects another
cache directory. A local override intentionally bypasses the pin and fetching.
Updating the harness requires changing the revision and checksums together;
branch heads and tags are never used as dependency versions.
