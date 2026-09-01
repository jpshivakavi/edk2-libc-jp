# Lab sign-off: GCC FULL on `feature/python-3.12.13-vs2022` (2026-09-01)

**Branch:** `feature/python-3.12.13-vs2022` · **Commit:** **`dbc8416c`** (includes **`edk2main.c`** GCC **`py312_boot_print_ascii`** fix)  
**Build:** WSL Ubuntu 20.04, **`build -t GCC -b NOOPT`**, **`BUILD_PYTHON312` + `BUILD_PYTHON312_FULL=TRUE`**  
**Layout:** `PACKAGES_PATH=$HOME/src/edk2-py312/edk2:$EDK2_LIBC_PATH` (same as historical GCC AppPkg work)  
**Package:** `create_python_pkg.sh GCC NOOPT X64`  
**Image:** FULL `Python312.efi` — GCC entry (**`edk2_switch_stack` + `py_install_idt`**), not MSVC 368 path

---

## Build notes (first GCC FULL on vs2022 tip)

| Issue | Fix |
|-------|-----|
| **`py312_boot_print_ascii` redefinition** | **`dbc8416c`**: define in **`edk2main.c`** only when **`PY_UEFI_BOOT_TRACE`** (MSFT-only in INF); GCC uses **`py312boot.h`** inline stub |
| **Missing `frozen_modules/*.h`** | Copy/regen per [`Python312_WSL_GCC_Build_Guide.md`](../Python312_WSL_GCC_Build_Guide.md) §6 (not committed in git) |

---

## Console output vs VS2022

GCC builds **do not** pass **`-DPY_UEFI_BOOT_TRACE=1`** in **`Python312.inf`** (MSFT only). Expect **`Python312: UefiMain`**, **`Python312: enter main`**, then script output — **not** the long MSVC boot ladder. Normal; see deviations **§11** / migration status **§ Boot trace**.

---

## Manufacturing smoke (hardware — passed 2026-09-01)

Each: **`-S -c "…"`** → **`Shell>`** → **`exit`** → BIOS/setup, **no hang**.

| Command | Result |
|---------|--------|
| `import sys; print('ok')` | OK |
| `import zlib; print(zlib.__name__)` | OK |
| `import hashlib; print(hashlib.sha256(b'x').hexdigest()[:8])` | OK |
| `import ctypes; print(ctypes.sizeof(ctypes.c_void_p))` | **`8`** (X64) |
| `import ssl; print(ssl.__file__)` | **`.../ssl/__init__.py`** (UEFI package) |
| `import ssl; ssl.create_default_context(); print('ok')` | OK |
| `import zlib, ssl, ctypes, hashlib; print('phase8 ok')` | OK |

---

## Meaning

**VS2022-track PyMod** (ssl package, OpenSSL RNG, teardown) does **not** break **GCC FULL** manufacturing on the **same branch**. Host GCC toolchain was **unchanged** from prior AppPkg work (toolchain upgrade deferred).

**Open:** FULL interactive REPL smoke on GCC image; optional git tag; pre-upstream-push cleanup.
