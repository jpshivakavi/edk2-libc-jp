# Phase 8.5 — Enable `_ctypes` / `ctypes` (vendored libffi)

After **8.1** zlib and **8.2** readline. **Before** OpenSSL **8.3–8.4** on this fork.

| Piece | Location |
|-------|----------|
| libffi (UEFI X64) | **`PyMod-3.12.13/Modules/libffi/`** |
| `_ctypes` + UEFI deltas | **`PyMod-3.12.13/Modules/_ctypes/*.c`**, `ctypes.h` |
| INF | `Python312.inf` libffi + `_ctypes` `[Sources]` |
| Built-ins | `PyMod-3.12.13/Modules/config.c` — `_ctypes`, `_ctypes_test` |

**Reference:** edk2-py312 `edk2-libffi` @ **`1fcd48b`** (LibFFI.inf file list) and
`edk2-cpython/Modules/_ctypes/` (UEFI `#if UEFI_C_SOURCE` / `FUNCFLAG_EFICALL`).

Build with **`edksetup.sh` in `~/src/edk2-py312/edk2`** (see WSL GCC guide §7).

**No** `LibFFI.dec` / extra `PACKAGES_PATH` segment.

Details: `PyMod-3.12.13/Modules/libffi/README.txt`

---

## Build / smoke

`PACKAGES_PATH=<edk2>:<edk2-libc>` only. StdLib patches + `srcprep.py`, then:

```bash
build -a X64 -b NOOPT -t GCC -p $EDK2_LIBC_PATH/AppPkg/AppPkg.dsc -D BUILD_PYTHON312
./create_python_pkg.sh GCC NOOPT X64 ~/py312_efi
```

```python
import ctypes
ctypes.c_int(42)
import _ctypes_test   # optional
```

`dlopen` / shared libraries are stubbed via `PyMod-3.12.13/efi/src/dummy_dlfcn.c`.

---

## Refresh vendored libffi

Copy sources listed in `edk2-libffi/EFI/LibFFI/LibFFI.inf` into `Modules/libffi/`
and headers from `edk2-libffi/EFI/LibFFI/Include/`.

Refresh `_ctypes` UEFI files from `edk2-cpython/Modules/_ctypes/` when syncing the port.
