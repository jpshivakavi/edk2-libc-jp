# Phase 8.5 — Enable `_ctypes` / `ctypes` (vendored libffi)

After **8.1** zlib and **8.2** readline. **Before** OpenSSL **8.3–8.4** on this fork.

| Piece | Location |
|-------|----------|
| libffi (UEFI X64) | **`PyMod-3.12.13/Modules/libffi/`** |
| `_ctypes` + UEFI deltas | **`PyMod-3.12.13/Modules/_ctypes/*.c`**, `ctypes.h` |
| INF | `Python312.inf` libffi + `_ctypes` `[Sources]` |
| Built-ins | `PyMod-3.12.13/Modules/config.c` — `_ctypes`, `_ctypes_test` |

**Standard reference (external deps):** always diff against the matching tree under
**`~/src/edk2-py312`** (or `edk2-py312/` on Windows) — do not guess INF flags or
file lists from upstream CPython alone.

| Vendored piece | edk2-py312 submodule / path |
|----------------|-------------------------------|
| libffi sources + include paths | **`edk2-libffi`** @ **`1fcd48b`** — `EFI/LibFFI/LibFFI.inf`, `EFI/LibFFI/Include/`, `EFI/LibFFI/libffi/include/`, `src/x86/asmnames.h` |
| `_ctypes` UEFI deltas | **`edk2-cpython/Modules/_ctypes/`** (`UEFI_C_SOURCE`, `FUNCFLAG_EFICALL`) |

AppPkg mirrors **`LibFFI.inf`**: `CC_FLAGS` and **`PP_FLAGS`** both use `include/` +
`libffi/include/` (needed for `unix64.S` / `fficonfig.h`). No `malloc_closure.c` on GCC
(same as **`edk2-cpython/efi/PythonPkg/PythonExtLib.inf`**).

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

From **`edk2-py312/edk2-libffi`** (commit **`1fcd48b`**):

1. `[Sources]` in `EFI/LibFFI/LibFFI.inf` → `PyMod-.../Modules/libffi/src/` (incl. `src/x86/*.S`, `asmnames.h`).
2. `EFI/LibFFI/Include/*` → `Modules/libffi/include/`.
3. `EFI/LibFFI/libffi/include/ffi_common.h`, `tramp.h` → `Modules/libffi/libffi/include/`.
4. `[BuildOptions]` `GCC:*_*_X64_PP_FLAGS` → same `-I` pair as `LibFFI.inf` (no
   `-fcf-protection=none` — not supported on GCC 5.x EDK toolchains). **`include/edk2_libffi_asm.h`**
   included from **`unix64.S` / `win64.S`** to strip `_CET_ENDBR` under `PP_RESP`.

Refresh `_ctypes` from **`edk2-py312/edk2-cpython/Modules/_ctypes/`** when syncing.
