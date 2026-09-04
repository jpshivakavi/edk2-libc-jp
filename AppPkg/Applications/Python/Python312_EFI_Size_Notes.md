# Python 3.12.13 UEFI EFI size vs Python 3.6.8

This note explains why **Python312.efi** is typically about **~10 MB** (NOOPT / GCC)
while the legacy **Python.efi** (3.6.8) is often **~4.5–5 MB** in the same AppPkg
style port. The gap is expected; it is not caused by Phase 7.5 (PyMod cleanup) or
by `create_python_pkg.sh` packaging.

Related:

- [`Python-3.12.13/Python312.inf`](./Python-3.12.13/Python312.inf)
- [`Python-3.6.8/Python368.inf`](./Python-3.6.8/Python368.inf)
- [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)

---

## What the `.efi` file contains

Both ports produce a **single statically linked UEFI application**: the CPython
runtime, built-in extension modules compiled into the image, UEFI glue, and
(edk2-libc) **StdLib** support linked via EDK II — not a small stub that loads
Python from elsewhere.

Pure Python under `EFI/lib/python3.12/` on the FAT volume is **packaged separately**
and does **not** count toward **Python312.efi** size.

---

## Observed sizes (reference)

| Artifact | Typical size | Build |
|----------|--------------|--------|
| Python.efi (3.6.8) | ~4.5–5 MB | AppPkg / GCC (varies by target and modules) |
| Python312.efi (3.12.13) | ~10–11 MB | `NOOPT` + `GCC` + `BUILD_PYTHON312` |

Example from a successful WSL build tree: packaged `Python312.efi` ~11 MB;
source file `PyMod-3.12.13/Python/deepfreeze/deepfreeze.c` ~5.1 MB on disk before compile.

---

## Main reasons 3.12 is larger

### 1. Deepfreeze (largest single factor)

CPython 3.12 embeds a large **frozen** import/bootstrap corpus in
`PyMod-3.12.13/Python/deepfreeze/deepfreeze.c` (plus `frozenmain.c` and `PyMod-3.12.13/Python/frozen_modules/*.h`).
That generated translation unit alone can be **on the order of ~5 MB** of C source.

Python 3.6.8 in AppPkg uses a minimal `Python/frozen.c` (hundreds of bytes to a few
KB in-tree), not a multi-megabyte deepfreeze blob.

`Python312.inf` lists:

- `PyMod-3.12.13/Python/deepfreeze/deepfreeze.c`
- `Python/frozen.c`
- `Python/frozenmain.c`

### 2. Larger interpreter core

3.12 brings a bigger runtime than 3.6: PEG parser (`pegen`, `string_parser`, …),
specialized bytecode (`specialize.c`, `ceval_gil.c`, …), instrumentation, richer
object model, and more internal headers/sources pulled into the monolithic INF.

Rough INF scale (`.c` entries in `[Sources]`):

| Port | ~`.c` files in INF |
|------|---------------------|
| Python368.inf | ~223 |
| Python312.inf | ~272 |

### 3. More built-in extension modules in Iteration 1 MIN

Examples linked in **3.12** Iteration 1 but absent or smaller in the **3.6** INF
layout:

| Module / area | 3.12.13 (typical) | 3.6.8 (typical AppPkg) |
|---------------|-------------------|-------------------------|
| `_decimal` + **libmpdec** | Many `.c` files | Not in Python368.inf |
| **`_hacl`** (MD5/SHA1/SHA2/SHA3) | Yes | Older per-algorithm `sha*` modules |
| `_zoneinfo`, `_asyncio`, subinterpreters | Often in MIN INF | Smaller / different set |

Iteration 1 **omits** `_ssl`, `_ctypes`, **zlib** (and GNU readline). Those are
present in some 3.6.8 builds (e.g. zlib as many `Modules/zlib/*.c` entries). So
3.12 would grow **further** when Phase 8 **vendored** OpenSSL/libffi (and zlib if not already in-tree) are enabled.

### 4. Build target: **NOOPT**

The documented AppPkg path uses **`-b NOOPT`**. Unoptimized object code is larger
than **RELEASE** (often materially so — exact ratio depends on toolchain and
sources). A RELEASE **Python312.efi** is a reasonable experiment if firmware
space is tight.

---

## What does **not** explain the size gap

| Change | Effect on `.efi` size |
|--------|------------------------|
| **Phase 7.5** (upstream stock tree + PyMod-only deltas) | No change to INF link set |
| **`create_python_pkg.sh`** | Copies `Lib/` and `Python312.efi` to `EFI/`; does not bloat the EFI link |
| **`srcprep.py`** | Overlays `.h`/`.py` for build and packaging; not a major C link contributor |

---

## Optional ways to reduce EFI size (later)

These are product/engineering trade-offs, not migration blockers:

1. **RELEASE build** — same INF, `-b RELEASE` after NOOPT is stable.
2. **Trim built-ins** — remove unused entries from `PyMod-3.12.13/Modules/config.c`
   and matching `[Sources]` in `Python312.inf` (e.g. `_decimal`, `zoneinfo`).
3. **Frozen / deepfreeze scope** — align with the minimal freeze set used in
   edk2-py312/edk2-cpython; smaller `deepfreeze.c` ⇒ smaller EFI (non-trivial
   maintenance).

---

## Quick verification commands (WSL)

```bash
# EFI produced by build or package
ls -lh ~/src/edk2-libc/Build/AppPkg/NOOPT_GCC/X64/Python312.efi
ls -lh ~/src/edk2-libc/AppPkg/Applications/Python/Python-3.12.13/py312_efi/EFI/bin/Python312.efi

# Deepfreeze source bulk
wc -c ~/src/edk2-libc-jp-vsfix/AppPkg/Applications/Python/Python-3.12.13/PyMod-3.12.13/Python/deepfreeze/deepfreeze.c

# Compare frozen stub (3.6 vs 3.12)
wc -c ~/src/edk2-libc/AppPkg/Applications/Python/Python-3.6.8/Python/frozen.c
wc -c ~/src/edk2-libc/AppPkg/Applications/Python/Python-3.12.13/Python/frozen.c
```

---

## Summary

**~2× the 3.6 EFI size is normal** for Python 3.12.13 in this port: deepfreeze
dominates, then a larger VM/parser and more static extensions, amplified by **NOOPT**.
Packaging and Phase 7.5 cleanup do not drive that difference.
