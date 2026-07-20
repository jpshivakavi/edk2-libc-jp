# Python 3.12.13 AppPkg — GCC vs VS2022 toolchain deviations

**Scope:** Same monolithic [`Python312.inf`](Python-3.12.13/Python312.inf), same vendored Phase 8 trees (zlib, OpenSSL, libffi layout under **`PyMod-3.12.13/Modules/`**), same **`PACKAGES_PATH=<edk2>;<edk2-libc-jp>`**. Differences below are **toolchain-specific** splits or **MSVC-on-UEFI** workarounds—not a different Python feature set.

**Related:**

- [`Python312_VS2022_Migration_Status.md`](Python312_VS2022_Migration_Status.md)
- [`Python312_Windows_VS2022_Build_Guide.md`](Python312_Windows_VS2022_Build_Guide.md)
- [`Python312_WSL_GCC_Build_Guide.md`](Python312_WSL_GCC_Build_Guide.md)
- Reference MSFT ctypes layout: [`Python-3.6.8/Python368.inf`](Python-3.6.8/Python368.inf)

---

## 1. Summary table

| Area | GCC (`-t GCC`) | VS2022 (`-t VS2022`) | Same runtime? |
|------|----------------|----------------------|-----------------|
| **INF source list** | ~95% shared | ~95% shared | Yes (same modules enabled in `config.c`) |
| **`_ctypes` / libffi** | Vendored **edk2-libffi** + `.S` | **`libffi_msvc`** (3.6.8-style) + **`win64.asm`** | Yes (`import ctypes`) |
| **OpenSSL libcrypto/ssl** | Shared `.c` list | Shared `.c` list + **`openssl_uefi_msvc.c`** | Yes (`hashlib`, `ssl`) |
| **zlib / `_ssl.c` / core Python** | Shared | Shared | Yes |
| **CPU probe asm** | `cpu_gcc.s` / `cpu_ia32_gcc.s` | `cpu.nasm` / `cpu_ia32.nasm` | Yes |
| **Profiling trampoline asm** | `Python/asm_trampoline.S` | *(not built)* | Minor (`perf_trampoline.c` still built) |
| **`_ctypes_test`** | Object linked (`\| GCC`) | Not compiled | **No** on UEFI either toolchain (not in `config.c` inittab) |
| **`pyconfig` / integer widths** | LP64: `SIZEOF_LONG` 8 | LLP64: `SIZEOF_LONG` 4, **`UEFI_MSVC_64`** | Same Python semantics |
| **Compiler flags / warnings** | `-Wno-error`, libffi `-I` on preprocessor | `/WX-`, `/Oi-`, many `/wd…`, **`LIBFFI_MSVC_*` `-I`** | N/A |
| **Packaging script** | `create_python_pkg.sh GCC …` | `create_python_pkg.bat VS2022 …` | Same **`EFI/`** layout |
| **Typical build flavor** | Often `NOOPT` on WSL | `RELEASE` on Windows | Size/optimize differ |

---

## 2. `Python312.inf` — sources by toolchain tag

EDK II builds each translation unit **once** for the active toolchain. Entries with **`| GCC`** or **`| MSFT`** are omitted from the other toolchain’s link.

### 2.1 GCC-only

| Source | Role |
|--------|------|
| `Python/asm_trampoline.S` | Assembler helper for **`perf_trampoline.c`** |
| `PyMod-…/Modules/_ctypes/_ctypes_test.c` | CPython ctypes test extension (see §5) |
| `PyMod-…/Modules/libffi/src/*.c` | Vendored libffi (closures, prep_cif, types, …) |
| `PyMod-…/Modules/libffi/src/x86/ffi64.c`, `ffiw64.c` | libffi x86-64 logic |
| `PyMod-…/Modules/libffi/src/x86/unix64.S`, `win64.S` | libffi call asm (with **`edk2_libffi_asm.h`** CET neutralizer on AppPkg GCC) |
| `PyMod-…/Modules/cpu_gcc.s` | `_Py_get_cpu_features` (X64) |
| `PyMod-…/Modules/cpu_ia32_gcc.s` | IA32 CPU probe |

**GCC `_ctypes`:** same PyMod `.c` files as MSFT, but linked against **vendored libffi**, not `libffi_msvc`. **No** `malloc_closure.c` on GCC (uses **`ffi_closure_alloc`** from vendored libffi — same as edk2-cpython **`PythonExtLib.inf`**).

### 2.2 MSFT-only

| Source | Role |
|--------|------|
| `Modules/_ctypes/libffi_msvc/prep_cif.c`, `types.c`, `ffi.c` | Legacy ctypes libffi port |
| `Modules/_ctypes/libffi_msvc/win64.asm` (X64), `win32.c` (IA32) | libffi call path |
| `PyMod-…/Modules/_ctypes/malloc_closure.c` | Closure allocation for MSFT libffi |
| `PyMod-…/Modules/cpu.nasm`, `cpu_ia32.nasm` | CPU probe |
| `PyMod-…/Modules/openssl/efi/src/openssl_uefi_msvc.c` | CRT/intrinsic/DSO link stubs (§4) |

**MSFT `_ctypes`:** `PyMod-…/Modules/_ctypes/{_ctypes,cfield,callproc,callbacks,stgdict}.c` — same as GCC, different libffi backend.

### 2.3 Shared (both toolchains)

Includes entire Python core, **`PyMod` EFI glue** (NASM **`edk2stack.nasm`**, **`edk2handler.nasm`** — not toolchain-tagged), **all vendored zlib `.c`**, **OpenSSL libcrypto + libssl** file list, **`Modules/_hashopenssl.c`**, **`Modules/_ssl.c`**, **`Parser/myreadline.c`**, etc.

**readline:** not GNU `readline.c`; **`pyreadline`** is staged at package time from **`PyMod-…/Modules/readline/`** (identical for GCC and VS2022 packages).

---

## 3. `[BuildOptions]` — flags and include paths

### 3.1 GCC (`GCC:*_*_*_CC_FLAGS`)

- **Includes:** `Include/`, `Include/internal/`, `PyMod-…/efi/Include`, HACL, **PyMod zlib**, **`LIBFFI_INC`** + **`LIBFFI_INT_INC`** (vendored libffi), OpenSSL **`efi/include`** + tree roots.
- **Defines:** `Py_BUILD_CORE`, `HAVE_MEMMOVE`, `NO_MSABI_VA_FUNCS`, `-Wno-error=…` (non-fatal warnings).
- **Extra:** `GCC:*_*_X64_PP_FLAGS` — libffi `-I` for **`unix64.S` / `win64.S`** preprocessing.

### 3.2 MSFT (`MSFT:*_*_*_CC_FLAGS`)

- **Includes:** same Python/OpenSSL/HACL/zlib **except libffi uses **`LIBFFI_MSVC_INC`** and **`LIBFFI_MSVC_PYMOD_INC`** only** (avoids picking up vendored **`ffi.h`** and breaking `_ctypes`).
- **Defines:** `UEFI_C_SOURCE`, `UEFI`, `Py_BUILD_CORE`, `HAVE_MEMMOVE`, `USE_PYEXPAT_CAPI`, `XML_*`, `NO_MSABI_VA_FUNCS`, etc.
- **MSVC-specific:** `/GL-`, **`/Oi-`** (no compiler intrinsics expansion — drives OpenSSL portable paths + **`openssl_uefi_msvc.c`**), **`/WX-`**, many **`/wd…`** suppressions.
- **Architecture macros (X64 / IA32 sections):** **`/DUEFI_MSVC_64`** or **`/DUEFI_MSVC_32`** → feeds **`pyconfig.h`** (§3.3).

### 3.3 `pyconfig.h` — LP64 (GCC) vs LLP64 (MSVC UEFI)

| Macro | GCC UEFI (typical) | VS2022 UEFI X64 |
|-------|--------------------|-----------------|
| `SIZEOF_LONG` | **8** | **4** (`_MSC_VER`) |
| `SIZEOF_SIZE_T` / `SIZEOF_VOID_P` | **8** | **8** (`UEFI_MSVC_64`) |
| `PLATFORM` | `"uefi"` | `"uefi"` |

Proof harness: [`Python-3.12.13/vs2022_verify/`](Python-3.12.13/vs2022_verify/) (`verify_pyconfig_gcc.sh`, `verify_pyconfig_msft.bat`).

**Source of truth:** `PyMod-3.12.13/Include/pyconfig.h` and `PyMod-3.12.13/efi/Include/pyconfig.h` → run **`srcprep.py`** after edits.

---

## 4. OpenSSL / libcrypto — same sources, MSVC-only glue

**Shared:** Full **`PyMod-…/Modules/openssl/**`** libcrypto + libssl source list in the INF (Phase 8.3/8.4), **`rand_efi.c`**, **`rand_rdrand.nasm`**, EFI **`eng_dyn.c`**, **`ui_openssl.c`**, etc.

**GCC behavior:** OpenSSL headers use **portable** rotate/byteswap macros when not on desktop MSVC fast paths; no desktop CRT.

**VS2022 deviations (PyMod overlays):**

| Topic | Change |
|-------|--------|
| **`openssl_uefi_msvc.c`** (`\| MSFT`) | Stubs: `_lrotl`, `_lrotr`, `_byteswap_ulong`, `_byteswap_uint64`, `strerror_s`, minimal **`DSO_*`** for `conf_mod.c` |
| **`opensslconf.h`** | **`OPENSSL_NO_DSO`**, **`OPENSSL_NO_UI`** for static UEFI libcrypto |
| **`err_all.c`** | `#ifndef OPENSSL_NO_DSO` / `NO_UI` around **`ERR_load_*`** |
| **`o_str.c`** | No **`strerror_s`** path when **`UEFI_C_SOURCE`** |
| **`aes_local.h`**, **`cast_local.h`**, **`rc5_local.h`**, **`modes_local.h`** | Skip MSVC intrinsic macros on **`UEFI_C_SOURCE`** / **`OPENSSL_SYS_UEFI`** |

**Not duplicated:** libssl APIs, `_hashopenssl.c`, `_ssl.c` — one copy, both toolchains.

---

## 5. Built-ins and `config.c`

**Same** UEFI extension set for both toolchains (`zlib`, `_ctypes`, `_hashlib`, `_ssl`, …) under **`#if UEFI_C_SOURCE`**.

**`_ctypes_test`:**

- **GCC INF:** compiles **`_ctypes_test.c`** (`| GCC`).
- **MSFT INF:** does not compile it.
- **`config.c`:** test module is **not** registered in **`_PyImport_Inittab`** on UEFI builds (guards are inside the UEFI block; **`import _ctypes_test`** is not supported on either packaged UEFI image today).
- **GCC** may still carry the test object in **`Python312.efi`** unless `/OPT:REF` strips it; functionally unused.

---

## 6. PyMod / core — `#if` guards (both toolchains, MSVC-triggered)

Most edits apply to **both** builds when **`UEFI_C_SOURCE`** is defined (GCC and MSFT set this on the AppPkg port). They exist because **MSVC defines `_WIN32` / `_MSC_VER`** without desktop CRT:

| Area | Representative files |
|------|----------------------|
| No `windows.h` / CRT | OpenSSL **`async_win.h`**, **`threads_win.c`**, **`e_capi.c`** |
| No `malloc.h` / tz | **`posixmodule.c`**, **`timemodule.c`** |
| Pickle / abort | **`_pickle.c`**, **`faulthandler.c`** |
| Math / hash / snprintf | **`pycore_pymath.h`**, **`pyhash.c`**, **`mysnprintf.c`**, **`dynamic_annotations.c`** |
| EFI exceptions | **`edk2excep.c`**, NASM stack, no **`intrin.h`** |

GCC benefits from the same guards (no accidental inclusion of desktop-only paths when porting).

---

## 7. deepfreeze / generated headers

**Shared** across toolchains (not tagged in INF):

- **`Python/deepfreeze/deepfreeze.c`** — must match **`Tools/build/generate_global_objects.py`** output (`pycore_global_strings.h`, runtime init, unicodeobject generated headers).
- One-character string refs in deepfreeze use **`_Py_SINGLETON(strings).ascii[N]`**, not stale **`&_Py_STR(dot)`**.

Regenerate after deepfreeze changes:

```text
python Tools/build/generate_global_objects.py
```

(from `Python-3.12.13/` tree.)

---

## 8. Build and packaging commands

| Step | GCC (WSL) | VS2022 (Windows) |
|------|-----------|------------------|
| Build | `build -t GCC -a X64 -b NOOPT … -D BUILD_PYTHON312` | `build -t VS2022 -a X64 -b RELEASE … -D BUILD_PYTHON312` |
| **`WORKSPACE`** | `edk2` clone | `c:\Users\njayapra\github\edk2` |
| **`EDK2_LIBC_PATH`** | fork path | `c:\Users\njayapra\github\edk2-libc-jp` |
| Package | `create_python_pkg.sh GCC NOOPT X64 out` | `create_python_pkg.bat VS2022 RELEASE X64 out` |
| EFI output path | `Build/AppPkg/NOOPT_GCC/X64/…/Python312.efi` | `Build/AppPkg/RELEASE_VS2022/X64/…/Python312.efi` |

**Packaging:** use **`create_python_pkg.bat`** / **`.sh`** from **`edk2-libc-jp`** only; plain **`edk2-libc`** may still ship a **Phase 6 stub** batch file.

**Deployed layout (identical):**

```text
EFI/bin/Python312.efi
EFI/lib/python3.12/          # Lib/ + PyMod Lib overlays + pyreadline
EFI/lib/python3.12/lib-dynload/   # empty (static extensions)
EFI/stdlib/etc/
```

---

## 9. What is intentionally *not* different

- Vendored **zlib**, **OpenSSL** source trees and **`BUILD_PYTHON312`** DSC switch.
- **`PREFIX` / `fs0:\EFI`** layout and **`create_python_pkg.*`** staging rules.
- **Phase 8** module list in migration docs (FULL port).
- **No** extra `PACKAGES_PATH` packages (LibFFI/OpenSSL/zlib sandboxes).
- **StdLib** patches: apply locally from **`patches/*.patch`**; not committed on branch (same policy as GCC AppPkg).

---

## 10. Maintenance rules

1. **Toolchain split:** new asm → **`| GCC`** vs **`| MSFT`** (or NASM vs `.S`) in **`Python312.inf`**; do not add global MSFT `-I` to vendored libffi on GCC.
2. **Shared header change** (OpenSSL, `pyconfig`, deepfreeze): run **GCC** and **VS2022** builds before merge to **`apppkg`**.
3. **MSVC-only link symbol:** prefer UEFI guards in OpenSSL headers first; add to **`openssl_uefi_msvc.c`** only when stubs are required.
4. **Document** new splits in this file and a line in **`Python312_VS2022_Migration_Status.md`** Session log.

---

*Last updated: 2026-07-20 (VS2022 FULL compile/link + packaging on `feature/python-3.12.13-vs2022`).*
