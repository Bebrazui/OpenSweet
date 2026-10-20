# STATUS

Plain-language state of the project: what is verified, what is in flight, what is
known to be missing. Keep it short and honest — update it before it drifts.

Last updated: 2026-10-19 (kernel `448 096` bytes, 65+ commits on `main`).

---

## What this is

A monolithic 64-bit x86_64 operating system written in pure flat assembler (FASM).
No C or Rust in the kernel. It boots from MBR through Stage 2 into Long Mode, brings
up a higher-half kernel, an ext4 read/write VFS, a compositing window manager, a
native ELF64 userland ABI, and a Linux-ABI compatibility layer capable of running
dynamically linked Debian binaries.

---

## Verified working

These are confirmed by the serial log of a real headless boot (`scripts/smoke.ps1`):

- MBR + Stage 2 boot, memory map, VBE framebuffer, Long Mode transition.
- Higher-half kernel, PMM/VMM, kernel heap, APIC timer, preemptive scheduler.
- ext4 mount (`ext4_ok=01`) on the second IDE drive.
- Native ELF64 loader loading `/terminal.elf` into Ring 3 (`Ring 3 process spawned`).
- Clean boot: no CPU exception, no page fault, no kernel panic.

## In flight (do not assume stable)

- **Debian `bash` under the Linux ABI.** `bash.deb` is packaged into the disk image
  and unpacked at runtime with `dpkg-deb -x`. Two major blockers have been fixed:
  - *Shared brk heap.* The heap used to be one identity-mapped window shared by
    every task, so the decompressor child spawned by `dpkg-deb` and its parent
    overwrote each other's allocator metadata; musl mallocng failed `get_meta()`
    and executed `a_crash()` (`hlt` -> #GP, exception 0D). Each slot now owns a
    private 32 MB physical heap (`HEAP_PHYS_BASE = 0x10000000`, above the PMM
    range) and `fork` deep-copies it.
  - *Truncated extraction.* Large sequential writes were placed into the first
    free holes in group 0, so one file needed more than the four entries of the
    ext4 inline extent root; `ext4_write_file` then failed `-EIO` or truncated
    the file, and the loader silently loaded fewer PT_LOAD segments. The
    allocator now starts each bitmap scan at an append-point hint
    (`ext4_alloc_block_hint` / `ext4_scan_alloc_watermark`), keeping a
    sequentially written file in a single extent. The earlier "group descriptor
    corruption" diagnosis was wrong: the wild block numbers came from reading a
    register clobbered by the ATA retry path.
  - *Result:* `dpkg-deb -x /bash.deb /` now extracts `/bin/bash` at its full
    `1 234 376` bytes, the loader reports `loaded segments: 4`, and
    `bash --version` prints the real GNU banner (5.1.4) and exits 0.
  - *Still open:* an interactive `bash` session (prompt / readline), pipelines,
    job control, and the `fstat`-on-`/dev/tty` path that currently returns
    `EBADF`. Reproduce with `scripts/test_bash.ps1` (use `-RebuildDisk` for a
    clean tree; a failed extraction leaves partial state behind).
- **Interactive children in the native terminal.** A foreground child now gets
  a stdin pipe as fd 0, and the terminal multiplexes the child's output with
  GUI events, so a Linux program can be typed at and it keeps repainting.
  - Kernel: `SYS_POLL` (38) / `vfs_poll` report real pipe readiness
    (`POLLIN`/`POLLOUT`/`POLLHUP`) and sleep in 1 ms slices until the timeout.
    The Linux ABI `poll` uses the same readiness instead of "everything is
    ready", which used to make poll loops burn CPU.
  - Terminal: a small ANSI subset (CSI cursor moves, EL, ED, OSC) is decoded,
    quotes are honoured when splitting argv, and bytes a child echoes back
    that match just-typed input are dropped so nothing prints twice.
  - *Limit:* a pipe is not a tty. `bash` therefore runs non-interactively (no
    `PS1`, no readline, no job control) and programs calling `isatty` take
    their pipe path. A real `/dev/ptmx` + `/dev/pts/N` with termios and a line
    discipline is the next step.
  - *Verified:* `scripts/test_term_input.ps1` types `echo GOT:sh` while
    `/bin/sh` runs and busybox prints `GOT:sh`; with `-Bash` it unpacks
    `bash.deb` and runs `/bin/bash -c 'echo from-bash-c'`.
- **X11 client (`ico`).** X11 wire protocol lives in `kernel/x11.inc`, SHM in
  `kernel/shm.inc`. Library set is packaged but the client is not confirmed running.
- **Linux ABI serial tracing.** `compat_linux.inc` prints `openat`/`fstat`/`lseek`/
  `mmap`/read/write events to COM1 on every call. This is a deliberate debugging aid
  while chasing `bash`, not production logging.

## Known gaps

Ordered roughly by how much they block other work.

1. **Portability of the full build.** The kernel assembles anywhere (see CI below).
   The **disk image does not**: `make-ext4.ps1` doubles as the userland build driver
   and invokes `tools/w64devkit/bin/gcc.exe` with Windows-target flags (`-mabi=sysv`).
   `tools/w64devkit/` is gitignored, so a fresh clone cannot produce `disk.img`.
2. **Test harness requires Python + Pillow.** All `scripts/test_*.py` and
   `scripts/test-*.ps1` screenshot-based tests need a Python install with PIL, which
   is not part of the documented toolchain. The new `scripts/smoke.ps1` has no such
   dependency.
3. **No address-space hardening.** Per-task PML4 exists and CR3 is switched, but
   `fork` is a deep copy (no COW), SMEP/SMAP are not enabled, and the user virtual
   layout is fixed. Running untrusted binaries relies entirely on range checking in
   `sys_validate_user_ptr`.
   The window-canvas range (`0x00800000..0x01000000`) is still identity-mapped and
   therefore shared between tasks; only the brk heap was made private.
   Syscalls also run with interrupts disabled (`syscall_entry`): the ext4/ATA state
   is a single shared set of globals, so preempting a syscall in the middle of a
   filesystem operation corrupts it. Blocking syscalls still schedule through an
   explicit `int 0x31`. A per-subsystem lock (with preemption left on) is the
   correct long-term replacement.
4. **Linux ABI depth.** `epoll` (213/232/233/291), `select`/`pselect6`/`ppoll`,
   `gettimeofday`, `clock_gettime`, `nanosleep`, `futex`, `shm*`, `sendfile`,
   `statx`, `renameat2` and the usual network calls are implemented. Recently added:
   `pipe2(293)`, `dup3(292)`, `set_robust_list(273)`, `get_robust_list(274)`,
   `prlimit64(302)`, `getrlimit(97)`, `setrlimit(160)`, `getrandom(318)`,
   `getcpu(309)`, `fadvise64(221)`, `readahead(187)`, `membarrier(324)`,
   `sched_getaffinity/setaffinity(203/204)`, `statfs/fstatfs(137/138)`.
   Missing and known to matter: `inotify` (253–255), `rseq(334)` (returns ENOSYS
   on purpose), real `futex` time64 robustness, `signalfd`, `timerfd`.  See
   `docs/linux-syscalls.md`.
   The per-task fd table was also raised from 16 to 64 entries.
5. **`make-ext4.ps1` is a single point of failure.** ~30 KB of hand-rolled ext4
   construction with hard-coded inode numbers (32, 36–46). A silent accounting error
   produces a corrupt image with no diagnostic.
6. **Documentation drift.** `README.md` still calls ext4 "read-only" (it writes) and
   omits the network stack, X11 and userland from the tree; `docs/ROADMAP.md` lists
   networking as not started. `gemini.md`, `docs/linux-syscalls.md` and
   `docs/linux-abi-roadmap.md` are stored in a broken encoding and render as mojibake.
7. **Hygiene.** No `LICENSE`. Two dead string constants in `compat_linux.inc`
   (`str_l_write_dbg`, `str_l_write_dbg2`) are declared but never used.
   `make-disk.cmd` references an absolute path under `C:\Users\ttt79\`.

---

## Building

Toolchain paths are overridable via the environment; defaults match the original
development machine.

```
set FASM=C:\tools\fasm\FASM.EXE        REM optional override
set QEMU=C:\Program Files\qemu\qemu-system-x86_64.exe

build.cmd        REM assembles boot.bin, stage2.bin, kernel.bin -> build\os.img
make-ext4.ps1    REM builds userland .elf files and -> build\disk.img
run.cmd          REM boot in QEMU with a window (serial -> build\serial.log)
```

## Testing

```
powershell -File scripts/smoke.ps1              REM boot build/ headless and assert on serial
powershell -File scripts/smoke.ps1 -Build       REM rebuild first, then smoke
powershell -File scripts/smoke.ps1 -Require 'x' REM negative check: should exit 1
```

`smoke.ps1` exits `0` only if all required markers are present and no fatal marker
(exception / page fault / panic) was found. It needs only PowerShell and QEMU.
It does **not** yet assert that the desktop renders — screenshots remain a manual
step.

## CI

`.github/workflows/build.yml` assembles `boot.asm`, `stage2.asm` and `kernel.asm`
with the Linux build of flat assembler 1.73.35 and checks the artifact sizes. This
gives a regression gate for the kernel itself. The job has **not yet been executed
on a Linux runner** — treat the first green run as the proof.
