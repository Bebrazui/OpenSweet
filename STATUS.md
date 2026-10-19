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
  and unpacked at runtime with `dpkg-deb -x`. **Major blocker fixed:** the brk heap
  used to be one identity-mapped window shared by every task, so the decompressor
  child spawned by `dpkg-deb` and its parent overwrote each other's allocator
  metadata; musl mallocng failed `get_meta()` and executed `a_crash()` (`hlt` ->
  #GP, exception 0D). Each slot now owns a private 32 MB physical heap
  (`HEAP_PHYS_BASE = 0x10000000`, above the PMM range) and `fork` deep-copies it.
  With that, `dpkg-deb` no longer crashes and extracts a real `/usr/...` tree.
  **Still open — group descriptor corruption.** When the extractor creates enough
  files that inode allocation reaches a second block group, `ext4_inode_load`
  reads `bg_inode_table_lo` from that group's descriptor as `0xFFFFFFFF` (the
  descriptor on disk is not valid), computes a wild block number and hands it to
  `ext4_blk_read`. The 28-bit ATA LBA then truncates it and reads an unrelated
  sector. Diagnostics now print `!blk wild ra=<caller> blk=<block>` for this, and
  the ATA layer retries once (`!ata retry lba=<lba> st=<status>`) so a single
  failure no longer aborts an extraction. The real fix is to make the writer keep
  `bg_inode_table` valid for every group it allocates inodes from (or to stop
  allocating inode numbers outside the group whose table exists).
  Reproduce with `scripts/test_bash.ps1` (use `-RebuildDisk` for a clean tree; a
  failed extraction leaves partial state behind).
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
4. **Linux ABI depth.** `gettimeofday(96)`, `clock_gettime(228)` and
   `nanosleep(35)` were added recently (`nanosleep` had a constant but no dispatch).
   Still missing for glibc/`bash`/`apt`/`python`: `epoll` (232/233/291),
   `inotify` (253–255), `pipe2(293)`, `dup3(292)`, `set_robust_list(274)`.
   See `docs/linux-syscalls.md`.
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
