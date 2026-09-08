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
| **Typical build flavor** | Often `NOOPT` on WSL | `NOOPT` (lab sign-off); `RELEASE` also builds | Size/optimize differ |
| **UEFI firmware entry** | **`edk2_switch_stack`** + **`py_install_idt`**, then **`ShellCEntryLib`** | **Same switch onto 64 MB** since 2026-09-08; **IDT still skipped** (`PY_UEFI_MSVC_IDT` opts in) | **Stack: yes. Faults: no** — **§11.1** |
| **Boot trace verbosity** | **`PY_UEFI_BOOT_TRACE`** not on GCC **`CC_FLAGS`** — short console (UefiMain, enter main) | **`PY_UEFI_BOOT_TRACE=1`** on MSFT — long ladder | N/A (debug only) |
| **Interactive REPL (manufacturing)** | Post–**`59000200`**: **stub readline** policy in tree | **Stdio REPL** signed off; pyreadline **opt-in** only | **Observed** divergence — **§11** |

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

**VS2022 UEFI X64 + `import ctypes`:** Firmware builds set **`UEFI_MSVC_64`** (from **`Python312.inf`** **`[BuildOptions.X64]`**) but often **not** **`_WIN64`**. Legacy **`libffi_msvc/types.c`** otherwise builds **`ffi_type_pointer`** as **4** bytes while Python **`struct.calcsize("P")`** is **8** → **`SystemError: sizeof(py_object) wrong: 4 instead of 8`** in **`Lib/ctypes/__init__.py`**. Fix: **`types.c`** / **`ffitarget.h`** / **`ffi.h`** treat **`UEFI_MSVC_64`** like Win64 (8-byte pointers, **`ffi_arg`**, trampolines).

### 2.3 Shared (both toolchains)

Includes entire Python core, **`PyMod` EFI glue** (NASM **`edk2stack.nasm`**, **`edk2handler.nasm`** — not toolchain-tagged), **all vendored zlib `.c`**, **OpenSSL libcrypto + libssl** file list, **`Modules/_hashopenssl.c`**, **`Modules/_ssl.c`**, **`Parser/myreadline.c`**, etc.

**readline:** not GNU `readline.c`; **`pyreadline`** is staged at package time from **`PyMod-…/Modules/readline/`** (identical for GCC and VS2022 packages).

---

## 3. `[BuildOptions]` — flags and include paths

### 3.1 GCC (`GCC:*_*_*_CC_FLAGS`)

- **Includes:** `Include/`, `Include/internal/`, `PyMod-…/efi/Include`, HACL, **PyMod zlib**, **`LIBFFI_INC`** + **`LIBFFI_INT_INC`** (vendored libffi), OpenSSL **`efi/include`** + tree roots.
- **Defines:** `Py_BUILD_CORE`, `HAVE_MEMMOVE`, `NO_MSABI_VA_FUNCS`, `-Wno-error=…` (non-fatal warnings). **`UEFI_C_SOURCE`** is **not** duplicated on the **`GCC:*_*_*_CC_FLAGS`** line in **`Python312.inf`** / **`Python312_MIN.inf`** (MSFT sets **`/DUEFI_C_SOURCE`** explicitly); AppPkg / StdLib still typically define **`UEFI_C_SOURCE`** for libc and Python when built under **`AppPkg.dsc`** — verify with **`build -v`** if behavior differs from MSFT.
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

- **`PyMod-3.12.13/Python/deepfreeze/deepfreeze.c`** — must match **`PyMod-3.12.13/Tools/build/generate_global_objects.py`** output (`pycore_global_strings.h`, runtime init, unicodeobject generated headers).
- **`PyMod-3.12.13/Python/frozen_modules/*.h`** and **`PyMod-3.12.13/Python/frozen.c`** — committed for clone-and-build; regen when frozen **`.py`** inputs change.
- After **`deepfreeze.py`**, run **`PyMod-3.12.13/Tools/build/fix_deepfreeze_latin1.py`** so single-char **`&_Py_ID`** become **`_Py_LATIN1_CHR`** (fork policy; avoids **C2039** / **`Py_DEBUG`** asserts).

Regenerate after deepfreeze or global-header changes:

| Host | Command |
|------|---------|
| **Windows** | **`Tools\build\regen_frozen_windows.cmd`** (writes PyMod paths; freeze → deepfreeze → **`fix_deepfreeze_statically_allocated.py`** → globals → **`fix_deepfreeze_latin1.py`**) |
| **Manual** | Same order as the batch file — **never** **`generate_global_objects.py`** alone if **`deepfreeze.c`** was not latin1-fixed |

**Fresh clone:** no regen step — see WSL / Windows build guides §6.

Details: [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §5.

One-character string refs may also use **`_Py_SINGLETON(strings).ascii[N]`**, not stale **`&_Py_STR(dot)`**.

---

## 8. Build and packaging commands

| Step | GCC (WSL) | VS2022 (Windows) |
|------|-----------|------------------|
| Build | `build -t GCC -a X64 -b NOOPT … -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE` | `build -t VS2022 -a X64 -b NOOPT … -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE` |
| **`WORKSPACE`** | `edk2` clone | `c:\Users\njayapra\github\edk2` |
| **`EDK2_LIBC_PATH`** | fork path | `c:\Users\njayapra\github\edk2-libc-jp-vsfix` |
| Package | `create_python_pkg.sh GCC NOOPT X64 out` | `create_python_pkg.bat VS2022 NOOPT X64 out` |
| EFI output path | `Build/AppPkg/NOOPT_GCC/X64/…/Python312/…/Python312.efi` | `Build/AppPkg/NOOPT_VS2022/X64/…/Python312/…/Python312.efi` |

**This table is FULL** (doc scope §0). **`BUILD_PYTHON312_FULL=TRUE`** is **required** on both
toolchains — the DSC defaults it to **FALSE**, which builds **`Python312_MIN.inf`** instead
(module dir **`Python312_MIN\`**). Flavor must match between **`build -b`** and the packaging
script; **`RELEASE`** works on VS2022 but **`NOOPT`** is what both toolchains lab-signed-off.

**Packaging:** use **`create_python_pkg.bat`** / **`.sh`** from **`edk2-libc-jp-vsfix`** (or any synced fork clone); plain **`edk2-libc`** may still ship a **Phase 6 stub** batch file.

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
2. **Shared header change** (OpenSSL, `pyconfig`, deepfreeze): run **GCC** and **VS2022** builds on **`feature/python-3.12.13-vs2022`** before push/PR (sole manufacturing branch; **`apppkg`** is reference-only).
3. **MSVC-only link symbol:** prefer UEFI guards in OpenSSL headers first; add to **`openssl_uefi_msvc.c`** only when stubs are required.
4. **Document** new splits in this file and a line in **`Python312_VS2022_Migration_Status.md`** Session log.

---

## 11. UEFI runtime — where VS2022 **clearly deviates** from GCC

**Compile/link parity** (§1–§8) does **not** imply **identical firmware behavior**. As of **2026-07-23** (branch tip **`3814cf9a`**, user-verified on **VS2022 MIN** only):

### 11.1 Firmware entry (C / INF — MSVC-only today)

| Topic | GCC **`Python312.efi`** | VS2022 **`Python312.efi`** |
|--------|-------------------------|----------------------------|
| **`UefiMain` path** | **`edk2_alloc_environ`**, **`edk2_switch_stack`** (dedicated stack), **`py_install_idt`**, then **`ShellCEntryLib`** | **Same, except `py_install_idt` is skipped.** Handles are read from `g_edk2_globals` and the result parked in `switch_status`, because MSVC addresses `UefiMain`'s frame `rsp`-relative |
| **Why** | Reference 3.12 AppPkg design | Converged 2026-09-08. The old **`PY_UEFI_MSVC_368_ENTRY`** workaround existed because `edk2_switch_stack` hung under MSVC — a NASM ABI mismatch, since fixed. The IDT is skipped only because it was never validated here |
| **Stack headroom** | 64 MB | **64 MB** — was ~128 KB (firmware stack) before the fix |

**Implication:** stack limits and deep recursion now **match**. **Fault handling still differs** —
no custom IDT under MSVC, so an unhandled fault reports differently. Define `PY_UEFI_MSVC_IDT` to
close that gap; it is untested.

> ## RESOLVED 2026-09-08 — the deviation itself is gone. History below kept for the reasoning.
>
> VS2022 now takes the stack switch, and the hang is fixed on hardware: `import json` and
> `import logging` on `-c`, `import json` in the REPL then `exit()` then Shell `exit`, the Phase 8
> sweep, and pyreadline §5.3/§5.4 all clean, **none raising `MemoryError`**.
>
> **`edk2_switch_stack` hung under MSVC for two reasons, both fixed.** The NASM helpers took their
> arguments in **`rdi`/`rsi`** (System V) while MSVC passes them in **`rcx`/`rdx`**, so `rsp` was
> loaded from garbage — fixed with `PY_UEFI_MS_ABI` on `MSFT:*_*_*_NASM_FLAGS`. Then the switch
> moves `rsp` out from under a *running* `UefiMain`: GCC at `-O0` keeps a frame pointer so its
> locals stay reachable through `rbp`, but **MSVC has none and addresses them `rsp`-relative**, so
> `image`/`systab` resolved into the new stack — fixed by reading them from `g_edk2_globals`.
>
> **`PY_UEFI_MSVC_368_ENTRY` and `PY_UEFI_FIRMWARE_STACK_BUDGET` are removed.** So is the earlier
> `MemoryError` expectation: the `PyOS_CheckStack` fix made the overflow *survivable*, this one
> means there is no overflow.
>
> ---
>
> **CONFIRMED 2026-09-08 — this deviation is the root cause of the VS2022 Shell `exit` hang.**
> The warning above turned out to be exactly right, and it went unconnected for months. Quantified:
> the GCC branch allocates **`PY_UEFI_DEFAULT_STACK_SIZE` = `(64*1024*1024)`** (64 MB) at
> [`edk2main.c:215-216`](./Python-3.12.13/PyMod-3.12.13/efi/src/edk2main.c) via `malloc`, while the
> `PY_UEFI_MSVC_368_ENTRY` branch **returns at `:212`, before that code is ever reached** — so
> VS2022 runs the whole interpreter on the **UEFI firmware stack** (order of 128 KB for a Shell
> app). That is roughly a **500× difference in stack headroom** from the same commit.
>
> **Compounding defect — `PyOS_CheckStack()` is broken on precisely this path.**
> [`edk2main.c:249-256`](./Python-3.12.13/PyMod-3.12.13/efi/src/edk2main.c) compares `rsp` against
> `g_edk2_globals.stack`, which is **only assigned at `:216`** — after the 368 early return. In a
> VS2022 image it is **NULL**, so `rsp > 0` is always true and the function **always reports that
> the stack is fine**. `USE_STACKCHECK 1` is set (`PyMod-3.12.13/efi/Include/pyconfig.h:1816`) and
> the function is called from `Objects/object.c`, `Python/ceval.c:260` and `Python/pythonrun.c:1913`,
> so the guard is **wired up and actively lying** rather than merely absent.
>
> **Failure signature:** a deep import chain overflows the firmware stack and corrupts memory
> *below* it — outside the image. Python is undamaged and finalizes perfectly (the
> `PY_UEFI_BOOT_TRACE` ladder runs clean through `before return from UefiMain`, and the prompt
> returns), then Shell **`exit`** hands control to BDS, which touches the corrupted region and
> hangs. Measured boundary: `import re` (42 modules) is clean, `import json` (48, and a **package**
> so ~2 frames deeper) hangs. **Peak C-stack depth is the variable, not module count.**
>
> **`edk2stack.nasm` / `edk2handler.nasm` are not toolchain-tagged** (`Python312.inf:62,69`), so
> `edk2_switch_stack` is already compiled into the VS2022 image — the assembly is not the blocker.
> **ROOT ENABLER, 2026-09-08: the VS2022 stack switch never worked because the NASM uses the
> System V ABI.** `edk2_switch_stack`, `edk2_get_idtr` and `edk2_set_idtr` read their arguments
> from `rdi`/`rsi`, but their C prototypes are plain (non-`EFIAPI`) functions, so **MSVC passes
> them in `rcx`/`rdx`** — the switch set `rsp` from garbage. That is the "hangs inside
> `ShellCEntryLib`" failure, and it explains why the switch and the IDT seemed to fail *as a pair*:
> one root cause. Fixed with ABI-aware NASM behind `MSFT:*_*_*_NASM_FLAGS = -DPY_UEFI_MS_ABI`
> (**GCC codegen unchanged**); the 64 MB path is opt-in via `PY_UEFI_MSVC_STACK_SWITCH`.
> Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_nasm_abi_mismatch.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_nasm_abi_mismatch.md).
> Full analysis: [`Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md`](./Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md).
>
> **Partially fixed 2026-09-08 (safety, not capability) — hardware result mixed @ `c3819602`.**
> Non-interactive `import json` now raises `MemoryError: stack overflow` and **Shell `exit` reaches
> BIOS setup with no hang**; boot trace confirms the bound is live
> (`rsp=6A969618 limit=6A951618 budget=18000`). The interactive REPL kept hanging, but
> `stack_min_rsp` high-water instrumentation showed the clean and hanging runs bottom out **256
> bytes apart** — so that is a **separate bug, not a budget problem**. It tracks the **exit route**:
> REPL `exit()` goes through `Py_Exit()` and never returns through `Py_RunMain()`/`main()`.
> **Still open** — the leaked-ConInEx explanation was ruled out, since `PY_UEFI_PYREADLINE` is
> undefined in every INF and `console_in` is NULL for a stdio REPL. **Do not lower
> `PY_UEFI_FIRMWARE_STACK_BUDGET`** — `import re` clears `limit` by only 6 240 B under `-b NOOPT`,
> so 96 KB is nearly too tight.
> Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_FULL_stackcheck_fix.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_FULL_stackcheck_fix.md).
> `PyOS_CheckStack()` now derives a real
> bound on the 368 path from `rsp` at `UefiMain` entry, using the new `PY_UEFI_FIRMWARE_STACK_BUDGET`
> (96 KB default) and `PY_UEFI_STACK_MARGIN` (8 KB) in `edk2stack.h`, stored in a new
> `g_edk2_globals.stack_limit`. Deep imports on VS2022 should now raise
> **`MemoryError: Stack overflow`** instead of corrupting firmware memory. **The stack-size
> deviation itself is unchanged** — VS2022 still runs on the firmware stack, so `json`/`logging`
> remain unusable there until the stack switch is made to work under MSVC (candidate: switch the
> stack but skip `py_install_idt()`).
>
> *That candidate was the answer — see the RESOLVED banner at the top of this section. The switch
> works with the IDT skipped, once the NASM ABI is right.*

### 11.2 MSFT-only compile-time defines (MIN today)

Set on **`Python312_MIN.inf`** **`MSFT:*_*_*_CC_FLAGS`**, not on GCC:

- **`PY_UEFI_BOOT_TRACE=1`** (diagnostic **`Print()`** ladder)

Also MSFT-only, on **`MSFT:*_*_*_NASM_FLAGS`** in both INFs:

- **`PY_UEFI_MS_ABI`** — makes `edk2stack.nasm` / `edk2handler.nasm` read arguments from
  `rcx`/`rdx` instead of `rdi`/`rsi`. **Required**; without it `edk2_switch_stack` sets `rsp` from
  garbage. GCC is unaffected.

**`PY_UEFI_MSVC_368_ENTRY=1` was here until 2026-09-08** and is now removed from both INFs along
with its branch in `edk2main.c` — see the RESOLVED banner in §11.1.

### 11.3 Interactive REPL, pyreadline, and Shell **`exit`**

| Topic | GCC (`feature/python-3.12.13-vs2022`) | VS2022 (manufacturing sign-off) |
|--------|--------------------------------------|----------------------------------|
| **Historical apppkg lab** | **`import readline`** → **pyreadline**; REPL **Tab** / line editing | Same **package layout** on stick; **VS2022** with pyreadline caused **REPL `exit()` hang**, **Shell `exit` hang**, **second launch** failures |
| **Session 10 policy (source, both toolchains)** | **`main.c`**: skip auto **`readline`** unless **`PY_UEFI_PYREADLINE`** at **compile**; **`site.py`**: no **`enablerlcompleter`** on **`uefi`**; **`readline.py`**: stub unless shell **`PY_UEFI_READLINE=1`** | **User-verified** stdio REPL + Shell teardown; stub **`import readline`** safe |
| **Manufacturing default UX** | Stdio **`>>>`** (like **3.6.8**) — **not** auto pyreadline | Same |
| **GCC re-smoke 2026-09-01** | **`set PY_UEFI_READLINE 1`**, **`-S`**, **`import readline`**, history/Tab, teardown — **pass** | *(Superseded — VS2022 was un-smoked then; see the 2026-09-08 row)* |
| **Both toolchains 2026-09-07 @ `3afa03f5`** (smoke doc §5.0 phases 1–5) | **Pass** — incl. asserted stub default (**`_ReadlineStub`**, no **`pyreadline`**/**`edk2console`** loaded) and non-interactive opt-in | **HANG reproduces** — stub phases clean, but **`import readline`** (even non-interactively) leaves Shell **`exit`** hanging. [`Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md`](./Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md) |

| **Both toolchains 2026-09-08** @ `3ec592e1` (smoke §5.3 / §5.4) | **Pass** | **PASS — the hang is gone.** Opt-in pyreadline works non-interactively and interactively, with up-arrow history and Tab completion, and Shell **`exit`** reaches firmware |

**Takeaway (rewritten 2026-09-08):** the rows above are a **chronology, not a live warning**. The
VS2022 pyreadline hang recorded on 2026-09-07 **was never a readline defect** — it was the
System V / MS x64 NASM ABI mismatch keeping MSVC off the 64 MB stack (§11.1), and `pyreadline` was
merely a deep enough import to hit it. With that fixed, **both toolchains now behave the same for
interactive REPL with pyreadline**, so the old "do not claim parity here" instruction no longer
holds.

**Unchanged and unrelated to the hang:** **manufacturing default on both toolchains stays stdio.**
`import readline` is a no-op stub unless the shell sets `PY_UEFI_READLINE=1` — that is a deliberate
policy so a scripted run cannot inherit the console (§11.3 rows above, smoke §5.1–§5.2), not a
workaround for a defect.

**`PY_UEFI_READLINE=1` alone does not enable line editing:** REPL still uses stdio until **`import readline`** (or compile **`PY_UEFI_PYREADLINE`**). Arrow keys without import → **`SyntaxError` … U+001B**.

**Correction (2026-09-07): the VS2022 hang is not a line-editing or REPL problem.** A one-shot **`Python312.efi -S -c "import readline, …"`** — no interactive session, no keystrokes — is enough to leave Shell **`exit`** hanging. Hook install (**`console.install_readline`** → **`install_readline_hook`**) plus **`rl.read_history_file()`** at import time is sufficient. Earlier text in this section describing it as a REPL/**`exit()`** issue understates the trigger.

**Optional pyreadline on VS2022 (development only):** shell env **`PY_UEFI_READLINE=1`** + **`import readline`** — **not** manufacturing-signed-off on VS2022. See [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10.

### 11.4 Shared UEFI teardown sources (both toolchains when `UEFI_C_SOURCE` active)

These apply to **both** images built from the same branch (not MSVC-specific), but were driven by **VS2022** failures:

| Component | Change |
|-----------|--------|
| **`edk2console.c`** | No 1 ms timer on init; detach drains **ConIn**, **`CloseProtocol`** on **ConInEx** |
| **`python.c` / `main.c`** | Clear readline hook + **`edk2_console_detach_readline()`** before finalize |
| **`pylifecycle.c`** | UEFI skips in **`Py_FinalizeEx`** (e.g. **`flush_std_files`** no-op) |
| **`readline.py` / `site.py`** | On-disk stdlib — must redeploy **`EFI\lib\python3.12\`**, not **`.efi`**-only |

### 11.5 Regression checklist (GCC after VS2022 runtime work)

1. WSL: **`BUILD_PYTHON312 -t GCC`**, **`BUILD_PYTHON312_FULL=TRUE`**, **`create_python_pkg.sh`**, deploy.
2. Phase 8 **`-S -c`** matrix + Shell **`exit`** — **done** 2026-09-01 (**`dbc8416c`**).
3. Default stdio **`Python312.efi -S`** (no pyreadline) + teardown — **GCC** + **VS2022** FULL **pass** 2026-09-01.
4. Optional pyreadline: **`set PY_UEFI_READLINE 1`**, **`-S`**, **`import readline`**, history/Tab, teardown — **GCC pass** 2026-09-01; document in migration **§ UEFI REPL / pyreadline**.
5. Re-run after shared PyMod/INF edits.
6. **Mandatory before touching either item in §11.8** — both are on the shared entry path that GCC
   has been signed off with, so a "cleanup" there is a GCC regression risk, not a no-op.

### 11.6 FULL **`import ssl`** / Shell **`exit`** — GCC reference vs VS2022 hang

| | **GCC FULL** (lab reference) | **VS2022 FULL** (reported hang) |
|--|------------------------------|----------------------------------|
| **`UefiMain` entry** | **`edk2_switch_stack`** + **`py_install_idt`**, then **`ShellCEntryLib`** | *Historical:* **`PY_UEFI_MSVC_368_ENTRY`**, Shell default stack only. Since 2026-09-08 both switch onto 64 MB (§11.1) |
| **`import ssl` + REPL `exit()`** | Completes; Shell **`exit`** returns to firmware | Often reaches **`before return from UefiMain`** then Shell **`exit`** or relaunch hangs |
| **Lab bisect (2026-08, VS2022 FULL)** | — | **`import sys`**: Shell **`exit`** OK. **`import _ssl`** / **`socket`**: OK. **`import ssl`** once (`-S -c`, **`ok`**, back to **`Shell>`**): Shell **`exit`** **hangs** (not cumulative — single run reproduces). WIP **`ssl.py`**: **`Purpose`** enum, **`socket` ∉ `sys.modules`**. |
| **Finalize (2026-08 WIP)** | Normal CPython teardown | Match GCC: full **`_PyModule_Clear`** / **`_ssl`** **`m_clear`**, GC, atexit; **`Lib/ssl/`** package with **`_uefi_min.py`** (no monolithic **`ssl.py`** import graph); **`py312_uefi_phase8_after_finalize`** → **`ERR_clear_error`** + **`edk2_console_handoff_to_shell`** when **`_ssl`** loaded |

**Takeaway:** Do not treat VS2022 ssl/Shell hangs as “OpenSSL on UEFI is broken.” **GCC FULL is the sign-off that Phase 8 + ssl can tear down.** VS2022 fix (2026-08): **`Lib/ssl/_uefi_min.py`** for **`os.name == 'uefi'`**, MSVC teardown aligned with GCC (drop skip-leak path), post-finalize OpenSSL/console handoff — see runtime notes §10.5.

**~~Longer-term VS2022 goal~~ — DONE 2026-09-08.** Fix **`edk2_switch_stack`** / alignment for MSVC so FULL can use the **same entry path as GCC** (see [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §4), then re-smoke **`import ssl`** without 368-only leaks. Both done: the entry paths are converged (§11.1) and **`import ssl`** / **`create_default_context()`** pass in the 2026-09-08 Phase 8 sweep on the switched stack.

### 11.7 **`ssl.create_default_context()`** — GCC OK, VS2022 hang (OpenSSL RNG ABI)

| | **GCC FULL** | **VS2022 FULL** (before fix) | **VS2022 FULL** (after fix, lab 2026-08-27) |
|--|--------------|------------------------------|-----------------------------------------------|
| **`import ssl; print('ok')`** | OK | OK | OK |
| **First `SSLContext()` / `create_default_context()`** | OK | Hang inside Python | OK — **`ok`**, **`Shell>`**, **`exit`** → BIOS |
| **OpenSSL entropy** | Same **`rand_efi.c`** + **`rand_rdrand.nasm`** in INF | Same sources, **different** NASM object ABI | **`win64`** NASM + **`uefi_urandom`** pool fill |
| **Link** | — | **`LNK2001`** **`OPENSSL_ia32_rdseed_bytes`** / **`rdrand_bytes`** if NASM lacked **`global`** exports | Link green |

**Why GCC is fine:** EDK **GCC** builds **`rand_rdrand.nasm`** as **elf64** — **`rdi`/`rsi`** match the assembly. **VS2022** builds **`win64`** — the old NASM used **`rdi`/`rsi`** while MSVC passes **`buf`/`len` in `rcx`/`rdx`**, so the byte **`loop`** ran with a garbage count → infinite stall the first time OpenSSL **`RAND`** pulled CPU seeding (**`SSL_CTX_new`** on **`create_default_context()`** ).

**Link failure:** **`rand_lib.c`** ( **`OPENSSL_RAND_SEED_RDCPU`** ) references **`OPENSSL_ia32_rdseed_bytes`** and **`OPENSSL_ia32_rdrand_bytes`**. NASM must declare **`global`** with those exact symbol names; otherwise **`rand_lib.obj`** fails at link with **`LNK2001`** even when assembly “looks” correct.

**Not only toolchain:** UEFI **`_ssl.c`** avoids Python 3.12’s **`@SECLEVEL=2:…`** default cipher string (OpenSSL 1.1.1f on firmware); **`Lib/ssl/_uefi_min.py`** avoids duplicating C-set verify flags. Primary stall was **NASM ABI + exports**; cipher/ctor tweaks are belt-and-suspenders.

**VS2022 fix (2026-08-27, PyMod):**

| File | Change |
|------|--------|
| **`Modules/openssl/efi/src/rand_rdrand.nasm`** | **`DEF_CPU_RANDOM`**: **`win64`** vs **elf64** args; **`global`** **`OPENSSL_ia32_rdseed_bytes`** / **`OPENSSL_ia32_rdrand_bytes`** |
| **`Modules/openssl/efi/src/rand_efi.c`** | Fill pools via **`uefi_urandom`** (EFI RNG) with correct entropy counts |
| **`Modules/_ssl.c`** | UEFI: **`HIGH:!aNULL:!eNULL:!MD5`** instead of **`PY_SSL_DEFAULT_CIPHER_STRING`** |
| **`Lib/ssl/_uefi_min.py`** | Client **`create_default_context`**: no redundant **`verify_mode`** / **`check_hostname`** |

**Lab sign-off:** [`Python312_VS2022_Lab/2026-08-27_VS2022_FULL_ssl_create_default_context_RNG.md`](./Python312_VS2022_Lab/2026-08-27_VS2022_FULL_ssl_create_default_context_RNG.md)

**Regression:** After changing NASM or **`rand_efi.c`**, re-smoke **VS2022** with **`import ssl; ssl.create_default_context(); print('ok')`** then Shell **`exit`**. **GCC** one-liner is cheap parity.

### 11.8 Open latent defects on the shared entry path (as of 2026-09-08)

Left deliberately unfixed when VS2022 moved onto the 64 MB stack. All three sit in code GCC has
been signed off with, so **each needs a GCC re-test (§11.5) and none should ride along with an
unrelated change.**

| # | Defect | Where | Why it was left |
|--:|--------|-------|-----------------|
| 1 | **Stack alignment expression does not align.** `stack + (stack % 512)` offsets the base by an arbitrary 0–511 bytes instead of rounding it up. MSVC needed a real alignment — `edk2_switch_stack()` leaves `rsp` at `base+size-0x200`, and a misaligned `rsp` faults MSVC's `movaps` spills — so the MSVC branch uses `(base + 511) & ~511`. **The GCC branch still has the original expression.** | `edk2main.c`, the `#ifdef _MSC_VER` alignment block | **Cannot fault, by arithmetic** — see below. It is a meaningless offset, not an alignment hazard, which makes this the **lowest-priority** of the three. Correcting it still changes the address GCC runs on, so it needs a re-test |
| 2 | **`edk2_alloc_environ()` is called twice**, once before the stack allocation and once after — and it was **not idempotent**, so the second call **leaked the first block**. | `edk2main.c` `:200` and `:212`; `efi/src/environ.c:25` | **FIXED 2026-09-08, awaiting a hardware re-test on both toolchains** — see below. The double call itself is retained deliberately |
| 3 | ~~**No custom IDT under MSVC.**~~ **CLOSED 2026-09-08** — verified working, then made the **default** via `/DPY_UEFI_MSVC_IDT=1` in **both** INFs. MSVC now installs the IDT like GCC always has. | `edk2main.c` | Was off pending proof it worked; it does (see below). Enabling it alongside the #4 fix means **both toolchains now report a fault and then spin** — one entry path, one fault behaviour |

**#1 in detail — why it cannot fault on GCC.** The earlier wording ("harmless *so far*", "`malloc`
returns well-aligned memory in practice") understated it: the safety is arithmetic, not luck.
`malloc` returns a **16-byte-aligned** base, and 512 is a multiple of 16, so `base % 512` is *also* a
multiple of 16 and `base + (base % 512)` stays 16-aligned. `size` (64 MB) and the `0x200` that
`edk2_switch_stack()` subtracts are both multiples of 16, so the resulting `rsp` is 16-aligned
regardless of what the offset came out to. **No `movaps` fault is reachable from this expression on
either toolchain.** What it actually does is move the stack base up by an unpredictable 0–511 bytes;
the `malloc` over-allocates by 1024, so it cannot overrun either. Real cost: it is misleading, and
the offset is not reproducible run to run. **Treat #1 as a correctness-of-intent cleanup, not a
latent crash.**

**#2 in detail — it is a per-run pool leak, not just a redundant call.** `edk2_alloc_environ()`
(`environ.c:25`) unconditionally `malloc`s `environ_size + environ_values_size` and assigns
`environ = (wchar_t**)env` with **no check for an existing `environ` and no free of the previous
one**. The second call therefore overwrote the pointer and **the first block was leaked outright**;
`edk2_free_environ()` frees only whichever block `environ` points at last. That block holds a copy of
every Shell environment variable, so it is a few KB, and **EFI pool memory is not reclaimed when the
image exits** — repeated `Python312.efi` invocations accumulate one leak each until reboot. Relevant
where the Shell runs the interpreter many times per boot. `malloc`'s result is also `memset` with no
NULL check.

**Fix applied 2026-09-08 — `edk2_alloc_environ()` now calls `edk2_free_environ()` on entry**, plus a
NULL check on the `malloc` before the `memset`. Two deliberate choices:

- **Idempotent alloc, not a deleted call site.** This fixes the *class* — any future double call is
  safe — instead of the one instance, and it preserves which block wins and when it is allocated, so
  the final state matches the signed-off images minus the leak. Deleting the first call remains
  available as a later cleanup; the double call is kept, with a comment, because both signed-off
  images are built that way.
- **Safe only because of read ordering.** `environ` is consumed by `posixmodule.c`'s
  `convertenviron()`, which runs long after the second call and **copies** the strings into Python
  objects rather than retaining pointers. `edk2main.c` is the only caller of either function. A
  future caller that frees while something holds a pointer into the block would not be safe.

**VERIFIED ON BOTH TOOLCHAINS 2026-09-08** at `3ec592e1` — GCC swept clean twice (on its own, then
again alongside the #4 fix) and VS2022 clean, including `import os; print(len(os.environ))` to
confirm the surviving block still populates. `efi/src/environ.c` compiles into both, so both were
required. Tag: `python312-both-toolchains-idt-fault-report-2026-09-08`.

**#3 in detail — VERIFIED WORKING under MSVC, 2026-09-08.** Built VS2022 FULL with
`/DPY_UEFI_MSVC_IDT=1` on the `MSFT:*_*_*_CC_FLAGS` line and ran the full sweep.

- **Install works.** Trace read `before py_install_idt` → `before ShellCEntryLib` → normal boot to
  `before Py_BytesMain`, so `sidt`, the 4 KB IDT copy and `lidt` all succeeded with the
  `PY_UEFI_MS_ABI` register fix. This was the only genuinely untested piece.
- **Restore works, and no regression.** The whole §3/§4/§5 sweep passed clean, reaching the
  `switched stack` line and exiting to firmware — which is what exercises `py_restore_idt()`, since
  the fault test itself never returns.
- **Fault reporting works.** `ctypes.cast(0x800000000000, POINTER(c_int))[0]` — non-canonical, so a
  #GP independent of how firmware mapped memory — printed
  **`Python312 boot: unhandled CPU exception 13`**. That string is reachable *only* through a
  trampoline written by `py_install_idt()`, and `13` is the predicted vector, so both the install and
  the vector routing are confirmed rather than inferred.

**Why the port was already MSVC-ready** (worth knowing before anyone "fixes" it): `edk2excep.h`
carries a `#pragma pack(push, 1)` MSVC struct that is byte-for-byte equivalent to the GCC bitfield
version (both 16 bytes, identical field offsets), and `py_install_idt()` **copies the firmware's live
IDT and patches only the three offset fields**, so the type/DPL/present bits come from the firmware's
own valid descriptors and the packed-vs-bitfield difference never has to produce them. The trampoline
is hand-assembled machine bytes, so it is toolchain-neutral.

**Now the default (2026-09-08), decided together with the #4 fix.** `edk2_seh_try()` /
`edk2_seh_catch()` **have no callers anywhere in the tree**, so `g_context_index` is always `-1` and
`py_handle_exception()` always takes the unhandled branch: report, `raise(signum)`, then
`while (exc_trap)` **spins forever**. Taken alone that is a poor trade — a fault the firmware would
report and usually reset becomes one line plus a hang. **What changed the decision is that GCC has
always behaved exactly this way**, so leaving MSVC deferring to firmware was not a safer default, it
was a second fault behaviour to reason about. With #4 fixed, both toolchains now report the vector,
`rip` and `cr2` and then spin, identically. `/DPY_UEFI_MSVC_IDT=1` is set in **both**
`Python312.inf` and `Python312_MIN.inf` so the MSVC entry path does not diverge again the way
`PY_UEFI_MSVC_368_ENTRY` did. Making faults *survivable* is separate work — it means wiring up the
dead `edk2_seh_*` path.

#### #4 — on GCC the IDT is a *silent* fault trap (found 2026-09-08)

GCC has always installed the IDT, but the only thing the handler says is behind
`#ifdef PY_UEFI_BOOT_TRACE` (`edk2excep.c:92`), and that macro is defined **only** on the `MSFT:`
`CC_FLAGS` line. So on a stock GCC image a CPU fault is caught by `py_handle_exception()`, reports
**nothing**, and spins forever in `while (exc_trap)`.

**That is worse than not installing the IDT at all.** It takes a fault the firmware would have
reported — and usually reset on — and converts it into a silent hang, while compiling out the single
diagnostic that would justify the trade. It is the mirror image of #3: VS2022 gets the reporting
decision, GCC gets the cost with none of the benefit and no way to opt out short of a rebuild.

**CONFIRMED on hardware 2026-09-08.** The §E fault one-liner
(`ctypes.cast(0x800000000000, POINTER(c_int))[0]`) produced **no message and a silent hang** on GCC,
where VS2022 with `PY_UEFI_MSVC_IDT` printed `unhandled CPU exception 13` on the identical test.
Silent-hang-with-no-firmware-output is the positive signal: a firmware-handled fault prints its own
dump, so the absence of *any* output means `py_handle_exception()` ran, caught the #GP, and had
nothing to say before entering `while (exc_trap)`. Same input, same handler, same spin — the **only**
difference between the two toolchains is whether the one `Print` was compiled in.

**FIXED AND VERIFIED ON GCC 2026-09-08** at `3ec592e1`: the same one-liner that previously hung
silently now prints `unhandled CPU exception 13` with `rip` and `cr2`, and the rest of the sweep is
green. Confirmed on the toolchain the defect was actually hurting, not by inference from VS2022.

The `Print` is now unconditional — a fault report is not debug tracing, and
the spin below it never returns, so gating it on a trace macro is what turned the fault into a silent
hang. Rejected alternatives: defining `PY_UEFI_BOOT_TRACE` for GCC too (drags in every other boot
line), and letting the fault reach firmware when there is no `edk2_seh_*` handler (a bigger
behaviour change, and it would re-split the two toolchains).

**The message now carries the fault location**, which is the actual point of reporting:

```text
Python312 boot: unhandled CPU exception 13 rip=<addr> cr2=<addr>
```

`SystemContext` is safe to dereference here — `edk2handler.nasm` is a port of EDK2's own
`ExceptionHandlerAsm.nasm` and passes a fully built `EFI_SYSTEM_CONTEXT_X64` (`mov rcx, [rbp + 8]`,
`mov rdx, rsp`, MS x64 convention). `rip` locates the faulting instruction; `cr2` is the faulting
address for a page fault and stale for anything else, `#GP` included.

**Watch the macro's scope, not just this instance.** This is the **second** time the `MSFT`-only
definition of `PY_UEFI_BOOT_TRACE` has hidden something from GCC — the `switched stack` measurement
was the first. Anything that is *evidence* rather than *tracing* does not belong behind it.

**Status: #2, #3 and #4 are all closed and verified on both toolchains** (`3ec592e1`, tag
`python312-both-toolchains-idt-fault-report-2026-09-08`). **#1 is the only one still open** —
cosmetic, and proven unable to fault. Procedure for the fault check is now in
[`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) §5.8 rather than only in this prose.

**Verified 2026-09-08 (VS2022):** `switched stack min_rsp=6486B0A8 limit=60877038 size=4000000` —
`size` is the required **`0x4000000`** (64 MB) and `min_rsp` sits **63.95 MB above `limit`**, so
`import sys` uses **~39.4 KB, 0.06 %** of the stack. The sweep did not pass narrowly. Procedure:
[`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) §1.

**GCC parity verified 2026-09-08** at `9db93ae1`, clean-tree FULL rebuild, full §3/§4/§5 sweep plus
the `json` / `logging` deep imports and the 23/42/48/65 `sys.modules` counts — all matching VS2022,
no `MemoryError`, every Shell `exit` clean. **The shared entry path is now signed off on both
toolchains.** Three GCC-visible deltas were under test: the rewritten `PyOS_CheckStack()` (now trips
at `base + PY_UEFI_STACK_MARGIN` rather than at `base`, so GCC's guard fires 8 KB earlier), a
non-zero `stack_limit` while Python runs, and the `edk2_globals_t` layout change — the last of which
makes a **clean rebuild mandatory**, since stale objects would read moved field offsets.

**No GCC depth number exists.** `PY_UEFI_BOOT_TRACE` is defined only on the `MSFT:` flags line, so a
stock GCC image prints no trace at all and the measurement above is VS2022-only. Getting the GCC
equivalent means adding `-DPY_UEFI_BOOT_TRACE=1` to `GCC:*_*_*_CC_FLAGS` for a throwaway build.
Note this also means **defect 1 below is not exonerated** by the GCC sweep: GCC passing proves only
that `malloc` happened to return a base the mis-derived offset left usable, which is what has always
been true.

**Also unverified: the MIN build on this path.** It carried `PY_UEFI_MSVC_368_ENTRY` too, so it moved
onto the switched stack without a hardware run — only FULL was swept.
See [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md).

#### NASM ABI audit — the same bug had already been found once and not generalised

§11.7 fixed **exactly this defect class** in `rand_rdrand.nasm` on **2026-08-27**: assembly reading
its arguments from `rdi`/`rsi` while MSVC passes them in `rcx`/`rdx`, producing an infinite stall.
Three weeks later the identical mistake in `edk2stack.nasm` cost a ten-round bisection through
Python-level behaviour (§11.1). **The lesson is to treat an ABI bug in one hand-written NASM file as
a signal to audit them all**, since the symptom — a hang far from the assembly — gives no hint of
where to look.

Audit as of 2026-09-08, for hand-written NASM taking C arguments:

| File | Built for | Status |
|------|-----------|--------|
| `PyMod-3.12.13/efi/src/edk2stack.nasm` | **both** (untagged in INF) | ABI-aware via `PY_UEFI_MS_ABI` |
| `PyMod-3.12.13/efi/src/edk2handler.nasm` | **both** (untagged) | ABI-aware via `PY_UEFI_MS_ABI` |
| `Modules/openssl/efi/src/rand_rdrand.nasm` | **both** | ABI-aware via `win64` (§11.7) |
| `PyMod-3.12.13/Modules/cpu.nasm` | **MSFT only** — `cpu_gcc.s` serves GCC | Written for MS x64; no hazard |
| `PyMod-3.12.13/Modules/cpu_ia32.nasm` | **MSFT only** — `cpu_ia32_gcc.s` serves GCC | IA32; no hazard |

**Every shared NASM file is now ABI-aware.** A new one must either be split by toolchain tag, like
`cpu.nasm`, or handle both conventions internally — anything else silently produces garbage
arguments on one toolchain.

---

*Last updated: 2026-09-08 (§11.1 entry paths converged; §11.8 open latent defects, NASM ABI audit, and GCC parity sign-off on the shared entry path).*
