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
| **UEFI firmware entry** | **`edk2_switch_stack`** + **`py_install_idt`**, then **`ShellCEntryLib`** | **`PY_UEFI_MSVC_368_ENTRY`**: **`ShellCEntryLib`** on default Shell stack only | **No** — see **§11** |
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

- **`Python/deepfreeze/deepfreeze.c`** — must match **`Tools/build/generate_global_objects.py`** output (`pycore_global_strings.h`, runtime init, unicodeobject generated headers).
- After **`deepfreeze.py`**, run **`fix_deepfreeze_latin1.py`** so single-char **`&_Py_ID`** become **`_Py_LATIN1_CHR`** (fork policy; avoids **C2039** / **`Py_DEBUG`** asserts).

Regenerate after deepfreeze or global-header changes:

| Host | Command |
|------|---------|
| **Windows** | **`Tools\build\regen_frozen_windows.cmd`** (freeze → deepfreeze → **`fix_deepfreeze_statically_allocated.py`** → globals → **`fix_deepfreeze_latin1.py`**) |
| **Manual** | Same order as the batch file — **never** **`generate_global_objects.py`** alone if **`deepfreeze.c`** was not latin1-fixed |

Details: [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §5.

One-character string refs may also use **`_Py_SINGLETON(strings).ascii[N]`**, not stale **`&_Py_STR(dot)`**.

---

## 8. Build and packaging commands

| Step | GCC (WSL) | VS2022 (Windows) |
|------|-----------|------------------|
| Build | `build -t GCC -a X64 -b NOOPT … -D BUILD_PYTHON312` | `build -t VS2022 -a X64 -b RELEASE … -D BUILD_PYTHON312` |
| **`WORKSPACE`** | `edk2` clone | `c:\Users\njayapra\github\edk2` |
| **`EDK2_LIBC_PATH`** | fork path | `c:\Users\njayapra\github\edk2-libc-jp-vsfix` |
| Package | `create_python_pkg.sh GCC NOOPT X64 out` | `create_python_pkg.bat VS2022 RELEASE X64 out` |
| EFI output path | `Build/AppPkg/NOOPT_GCC/X64/…/Python312.efi` | `Build/AppPkg/RELEASE_VS2022/X64/…/Python312.efi` |

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
| **`UefiMain` path** | **`edk2_alloc_environ`**, **`edk2_switch_stack`** (dedicated stack), **`py_install_idt`**, then **`ShellCEntryLib`** | With **`PY_UEFI_MSVC_368_ENTRY=1`** on **`Python312_MIN.inf`** / FULL MSFT flags: **`ShellCEntryLib`** on **Shell default stack**, **no** switch, **no** custom IDT |
| **Why** | Reference 3.12 AppPkg design; works on GCC in lab | Without 368 path, VS2022 **hung inside `ShellCEntryLib`** after stack switch (runtime notes §4) |
| **3.6.8 analogy** | N/A (368 always **`ShellCEntryLib`**) | Matches **3.6.8 VS2022** entry style |

**Implication:** deep recursion, fault handling, and stack limits may **differ** between GCC and VS2022 images even from the same git commit.

### 11.2 MSFT-only compile-time defines (MIN today)

Set on **`Python312_MIN.inf`** **`MSFT:*_*_*_CC_FLAGS`**, not on GCC:

- **`PY_UEFI_MSVC_368_ENTRY=1`**
- **`PY_UEFI_BOOT_TRACE=1`** (diagnostic **`Print()`** ladder)

GCC builds do **not** use the 368 entry workaround.

### 11.3 Interactive REPL, pyreadline, and Shell **`exit`**

| Topic | GCC (historical reference) | VS2022 (manufacturing sign-off) |
|--------|----------------------------|----------------------------------|
| **Phase 8 smoke** ([`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md)) | **`import readline`** → **pyreadline**; REPL **Tab** / line editing via **edk2console** | Same **package layout** on stick, but **VS2022** with pyreadline caused **REPL `exit()` hang**, **Shell `exit` hang**, **second launch** failures |
| **Session 10 policy (both toolchains in source)** | **`main.c`**: skip auto **`readline`** unless **`PY_UEFI_PYREADLINE`** at compile; **`site.py`**: no **`enablerlcompleter`** on **`uefi`**; **`readline.py`**: stub unless env **`PY_UEFI_READLINE=1`** | **User-verified** stdio REPL + Shell teardown; stub **`import readline`** safe |
| **Default UX after deploy** | **Not re-smoked** on WSL after **`59000200`** / **`3814cf9a`** — packaged GCC stick may now match **stdio REPL** even though earlier GCC sign-off used **pyreadline** | **Stdio `>>>`** (like **3.6.8**), no Tab completion unless experimental opt-in |

**Takeaway:** Do **not** claim “GCC and VS2022 behave the same in the Shell” for the **interactive REPL** path. **VS2022 manufacturing** intentionally **deviates** from the **historical GCC pyreadline** experience until a future change re-enables line editing without Shell hang.

**Optional pyreadline on VS2022 (development only):** shell env **`PY_UEFI_READLINE=1`** + optional **`/DPY_UEFI_PYREADLINE=1`** on MSFT **`CC_FLAGS`** — **not** manufacturing-signed-off. See [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10.

### 11.4 Shared UEFI teardown sources (both toolchains when `UEFI_C_SOURCE` active)

These apply to **both** images built from the same branch (not MSVC-specific), but were driven by **VS2022** failures:

| Component | Change |
|-----------|--------|
| **`edk2console.c`** | No 1 ms timer on init; detach drains **ConIn**, **`CloseProtocol`** on **ConInEx** |
| **`python.c` / `main.c`** | Clear readline hook + **`edk2_console_detach_readline()`** before finalize |
| **`pylifecycle.c`** | UEFI skips in **`Py_FinalizeEx`** (e.g. **`flush_std_files`** no-op) |
| **`readline.py` / `site.py`** | On-disk stdlib — must redeploy **`EFI\lib\python3.12\`**, not **`.efi`**-only |

### 11.5 Regression checklist (GCC after VS2022 runtime work)

1. WSL: **`BUILD_PYTHON312 -t GCC`**, **`create_python_pkg.sh`**, deploy.
2. Confirm: REPL → **`exit(0)`** → Shell → **`exit`**; note whether **`import readline`** loads stub or pyreadline per env.
3. Record result in **`Python312_VS2022_Migration_Status.md`** Session log (GCC parity line).

### 11.6 FULL **`import ssl`** / Shell **`exit`** — GCC reference vs VS2022 hang

| | **GCC FULL** (lab reference) | **VS2022 FULL** (reported hang) |
|--|------------------------------|----------------------------------|
| **`UefiMain` entry** | **`edk2_switch_stack`** + **`py_install_idt`**, then **`ShellCEntryLib`** | **`PY_UEFI_MSVC_368_ENTRY`**: **`ShellCEntryLib`** on Shell default stack only |
| **`import ssl` + REPL `exit()`** | Completes; Shell **`exit`** returns to firmware | Often reaches **`before return from UefiMain`** then Shell **`exit`** or relaunch hangs |
| **Lab bisect (2026-08, VS2022 FULL)** | — | **`import sys`**: Shell **`exit`** OK. **`import _ssl`** / **`socket`**: OK. **`import ssl`** once (`-S -c`, **`ok`**, back to **`Shell>`**): Shell **`exit`** **hangs** (not cumulative — single run reproduces). WIP **`ssl.py`**: **`Purpose`** enum, **`socket` ∉ `sys.modules`**. |
| **Finalize (2026-08 WIP)** | Normal CPython teardown | Match GCC: full **`_PyModule_Clear`** / **`_ssl`** **`m_clear`**, GC, atexit; **`Lib/ssl/`** package with **`_uefi_min.py`** (no monolithic **`ssl.py`** import graph); **`py312_uefi_phase8_after_finalize`** → **`ERR_clear_error`** + **`edk2_console_handoff_to_shell`** when **`_ssl`** loaded |

**Takeaway:** Do not treat VS2022 ssl/Shell hangs as “OpenSSL on UEFI is broken.” **GCC FULL is the sign-off that Phase 8 + ssl can tear down.** VS2022 fix (2026-08): **`Lib/ssl/_uefi_min.py`** for **`os.name == 'uefi'`**, MSVC teardown aligned with GCC (drop skip-leak path), post-finalize OpenSSL/console handoff — see runtime notes §10.5.

**Longer-term VS2022 goal:** Fix **`edk2_switch_stack`** / alignment for MSVC so FULL can use the **same entry path as GCC** (see [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §4), then re-smoke **`import ssl`** without 368-only leaks.

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

---

*Last updated: 2026-09-01 (§1 boot trace GCC vs MSFT; GCC FULL lab on vs2022 branch).*
