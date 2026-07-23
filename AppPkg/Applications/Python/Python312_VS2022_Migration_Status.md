# Python 3.12.13 AppPkg — VS2022 / Windows Migration Status

**Plan:** [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md)  
**Windows build guide:** [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md)  
**MIN build (two INF):** [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md)  
**UEFI runtime / VS2022 fixes:** [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md)  
**GCC vs VS2022 deviations:** [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md)  
**3.6.8 VS2022 walkthrough:** [`Python368_Windows_VS2022_Build_Guide.md`](./Python368_Windows_VS2022_Build_Guide.md)  
**GCC reference (FULL port):** [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md)  
**GCC regression build:** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)  
**Started:** 2026-07-18  
**Updated:** 2026-07-23 (**Session 10** — VS2022 MIN REPL + Shell **`exit`**: default **stdio REPL** (no pyreadline); **`import readline`** stub; see [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10)  
**Strategy:** MSVC peer to GCC FULL; same `PACKAGES_PATH=<edk2>;<edk2-libc>`; vendored libs stay in **`PyMod-3.12.13/Modules/`**  
**Branch:** `feature/python-3.12.13-vs2022` (from `feature/python-3.12.13-apppkg`)  
**Target repo:** `jpshivakavi/edk2-libc-jp` (push from **`edk2-libc-jp-vsfix`** when ready)  
**Windows WORKSPACE:** `c:\Users\njayapra\github\edk2` (tianocore/edk2 — `edksetup.bat`, `Build\`)  
**Libc clone / `EDK2_LIBC_PATH`:** `c:\Users\njayapra\github\edk2-libc-jp-vsfix` (active VS2022 workspace; branch **`feature/python-3.12.13-vs2022`**)  
**WSL GCC regression:** user-verified **`BUILD_PYTHON312`** + **`create_python_pkg.sh`** green after V2/V3 prep (2026-07-20)  
**MSVC reference INF:** [`Python-3.6.8/Python368.inf`](./Python-3.6.8/Python368.inf)  
**3.6.8 VS2022 CI:** [`.github/workflows/build-python-uefi-vs2022.yaml`](../.github/workflows/build-python-uefi-vs2022.yaml) (`BUILD_PYTHON368` only today)

Build gate: **`-p AppPkg/AppPkg.dsc`** with `PACKAGES_PATH` including the libc fork — **not** `-p %EDK2_LIBC_PATH%\AppPkg\AppPkg.dsc` alone.

---

## Current status (2026-07-22)

**Branch:** `feature/python-3.12.13-vs2022` · **Workspace clone:** `edk2-libc-jp-vsfix` · **Remote:** `jpshivakavi/edk2-libc-jp`

| Area | State |
|------|--------|
| **VS2022 MIN** (`Python312_MIN.inf`, default DSC) | **Build + UEFI runtime Done** — user smoke: **`-h`**, **`-S -c "import sys; print(sys.version)"`**, **`-S -c "print('ok')"`**, interactive REPL |
| **MSVC entry** | **`PY_UEFI_MSVC_368_ENTRY=1`** — **`ShellCEntryLib`** on Shell stack (no custom stack switch / IDT around libc); required for VS2022 today |
| **Frozen / deepfreeze** | Regenerated on Windows via **`Tools/build/regen_frozen_windows.cmd`** (host **3.12.x**); **`statically_allocated`** + **`fix_deepfreeze_latin1.py`** + **`generate_global_objects.py`** |
| **VS2022 FULL** (`BUILD_PYTHON312_FULL=TRUE`, `Python312.inf`) | **Link Done** (Session 6); **UEFI Phase 8 smoke** (zlib, ssl, ctypes) **not signed off** |
| **GCC regression** | Last green **2026-07-20**; **re-run recommended** after deepfreeze / global-header changes in this push |
| **V7 CI** | No **`build-python312-uefi-vs2022.yaml`** yet |
| **Debug scaffolding** | **`PY_UEFI_BOOT_TRACE`**, StdLib **`Main.c`** probes, **`Py_DEBUG 1`** in UEFI **`pyconfig.h`** — trim when FULL is stable |

**Artifacts:** `Build\…\edk2-libc-jp-vsfix\…\Python312_MIN\DEBUG\Python312.efi` · package with **`create_python_pkg.bat`** (finds MIN or FULL output).

---

## Overall progress

| Phase | Name | Status |
|-------|------|--------|
| V0 | Prerequisites and baseline capture | **Skipped** (optional 3.6.8 VS2022 diff deferred) |
| V1 | Windows host and EDK workspace | **Done** |
| V2 | `pyconfig.h` / `UEFI_MSVC_*` | **Done** ( **`vs2022_verify/`** proof ) |
| V3 | MSFT `[BuildOptions]` in `Python312.inf` | **Done** |
| V4 | Toolchain-split sources; FULL VS2022 link | **Done** (`Python312.efi` / module link green) |
| V5 | Packaging on Windows (`create_python_pkg.bat`) | **Done** (user **`myUEFIPy312`**; PREFIX volume-relative — see Session 7) |
| V6 | Runtime smoke (MIN → FULL) | **Partial** — **MIN Done** (2026-07-22, **`PY_UEFI_MSVC_368_ENTRY`**, Windows regen); **FULL** UEFI smoke pending |
| V7 | Docs and CI (`build-python312-uefi-vs2022.yaml`) | **Partial** (build guide, deviations doc, this status; no 3.12 CI) |
| V8 | Vendored FULL on VS2022 (8.1→8.2→8.5→8.3→8.4) | **Done** (same monolithic INF as GCC; MSFT-specific glue only) |

**Legend:** Not started · In progress · Partial · Blocked · Done · Skipped

### Mapping to GCC AppPkg phases

| GCC AppPkg | VS2022 |
|------------|--------|
| 0–1 Scaffold | **V1** workspace |
| 2 PyMod / pyconfig | **V2** + PyMod MSVC fixes in **V4** |
| 3 Frozen | **V1** (Windows copy / GCC tree) |
| 4 INF MIN | **V3–V4** MSFT options + splits |
| 5 DSC / patches | **V1** + same patch policy as GCC |
| 6 MIN smoke | **V4–V6** |
| 7 Docs / CI | **V7** |
| 8 Vendored FULL | **V8** (each batch + `build -t VS2022`) |

---

## Locked decisions (VS2022 track)

| Item | Choice |
|------|--------|
| Baseline | GCC **Phase 8 FULL** on `feature/python-3.12.13-apppkg` — do not regress |
| `PACKAGES_PATH` | `<edk2>;<edk2-libc>` only — no sandbox LibFFI/OpenSSL/zlib packages |
| INF | **`Python312_MIN.inf`** + **`Python312.inf`** via **`BUILD_PYTHON312_FULL`** in DSC (no `!if` inside INF); MSFT **`PY_UEFI_MSVC_368_ENTRY`** for runtime — see runtime notes |
| `pyconfig` source of truth | **`PyMod-3.12.13/Include/pyconfig.h`** + **`efi/Include/pyconfig.h`** → **`srcprep.py`** |
| MSVC sizing | **`/DUEFI_MSVC_64`** on X64; **`_MSC_VER`** LLP64 (`SIZEOF_LONG` 4); GCC **`#else`** LP64 (8) |
| StdLib patches | Apply **`patches/*.patch`** locally; **do not commit** `StdLib/` on branch |
| **`PREFIX` / getpath** | **`\\EFI`** relative to interpreter volume **`fsN:`** (3.6.8-style); not hard-coded **`fs0:`** — see Session 7 |
| ctypes on MSFT | **`libffi_msvc`** + **`| MSFT`** `_ctypes` sources (3.6.8 pattern); vendored **libffi `.S`** **`| GCC`** only |
| First MSVC target | **X64 RELEASE** (IA32 deferred) |
| Proof before large builds | **`vs2022_verify/`** after every **`pyconfig.h`** change |

---

## Locked paths (Windows host)

| Variable | Value |
|----------|--------|
| `EDK2_LIBC_PATH` | `c:\Users\njayapra\github\edk2-libc-jp-vsfix` |
| `WORKSPACE` | `c:\Users\njayapra\github\edk2` |
| `PACKAGES_PATH` | `c:\Users\njayapra\github\edk2;c:\Users\njayapra\github\edk2-libc-jp-vsfix` |
| `NASM_PREFIX` | `C:\NASM\` (NASM **3.02**) |

---

## Work log

### 2026-07-18 — Session 1 (V1 docs + host prep)

1. Created [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md), this status file, [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md).
2. Branch **`feature/python-3.12.13-vs2022`**; applied patches **0001–0004** locally; **`srcprep.py`**; frozen headers synced from WSL.
3. **`edksetup.bat`** + **`Edk2ToolsBuild.py -t VS2022`** (after **`pip install -r edk2/pip-requirements.txt`**).

### 2026-07-20 — Session 2 (V1 smoke + StdLib MSVC)

1. **`BUILD_PYTHON368`** VS2022 RELEASE/X64 **Done** — [`Python368_Windows_VS2022_Build_Guide.md`](./Python368_Windows_VS2022_Build_Guide.md).
2. MSVC **`/WX`** fixes in **`patches/0001`** (`upipe.c`), **`patches/0002`** (`daAnsi.c`, `daConsole.c`).
3. Documented **`NASM_PREFIX=C:\NASM\`**.

### 2026-07-20 — Session 3 (V2/V3 prep + GCC gate)

1. **`PyMod-3.12.13/Include/pyconfig.h`**: **`UEFI_MSVC_{32,64}`**, **`_MSC_VER`** **`SIZEOF_*`** / **`ALIGNOF_LONG`**.
2. **`Python312.inf`**: **`MSFT:*_*_*_CC_FLAGS`**, **`/DUEFI_MSVC_64`** on X64; GCC line unchanged.
3. User WSL: **`BUILD_PYTHON312`** + **`create_python_pkg.sh`** **Done** (GCC reference unchanged).

### 2026-07-20 — Session 4 (V2 proof)

1. Added **`Python-3.12.13/vs2022_verify/`** — compile-time checks for MSVC **`/DUEFI_MSVC_64`** and GCC **`#else`**.
2. **`verify_pyconfig_msft.bat`** → **OK**; **`verify_pyconfig_gcc.sh`** (WSL) → **OK**.
3. **Phase V2** closed per port plan exit criteria.

### 2026-07-20 — Session 5 (V4 start — not validated)

1. **`Python312.inf`**: **`| GCC`** on **`asm_trampoline.S`**, vendored **libffi** + **`_ctypes`**; **`| MSFT`** **`libffi_msvc`** / **`_ctypes`** block; **`[Sources.X64]`** / **`[Sources.IA32]`** cpu + asm splits.
2. Copied from **3.6.8**: **`Modules/_ctypes/libffi_msvc/`**, **`PyMod-.../libffi_msvc/`**, **`malloc_closure.c`**, **`cpu*.nasm`**, **`cpu*_gcc.s`**.
3. **`BUILD_PYTHON312 -t VS2022`** attempted — **no recorded pass/fail** (long run interrupted). **V3/V4** remain **Partial**.

### 2026-07-20 — Session 6 (V3/V4/V8 — FULL **`BUILD_PYTHON312`** VS2022)

**Gate command (same env as V1.7):**

```cmd
build -t VS2022 -a X64 -b RELEASE -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312
```

**Result:** Compile + link **Done** for monolithic **`Python312.inf`** (Phase 8 FULL parity with GCC — zlib, vendored OpenSSL **`_hashlib`/`_ssl`**, **`libffi_msvc`** **`_ctypes`**, same OpenSSL/zlib source lists as GCC).

#### A. `Python312.inf` / libffi (MSFT vs GCC)

| Change | Purpose |
|--------|---------|
| **`MSFT:*_*_*_CC_FLAGS`**: `/GL-` `/Oi-`, UEFI includes, **`/DUEFI_C_SOURCE`**, **`/WX-`**, **`/wd4201`** **`/wd4273`**; **`LIBFFI_MSVC_*`** on MSFT only (vendored **`libffi`** `-I` on **GCC** only) | Match **Python368** pattern; avoid wrong **`ffi.h`** on MSFT |
| **`| GCC`** vendored **`PyMod-…/Modules/libffi/**`** + **`asm_trampoline.S`** | GCC FULL ctypes |
| **`| MSFT`** **`Modules/_ctypes/libffi_msvc/`** (`prep_cif.c`, **`types.c`**, `ffi.c`), **`win64.asm`**, **`malloc_closure.c`**, PyMod **`_ctypes`** | MSFT ctypes + **`ffi_type_*`** + **`ffi_prep_cif_var`** |
| **`openssl_uefi_msvc.c | MSFT`** | Link stubs (see **D**) |
| **`[Sources.X64]`** **`cpu.nasm | MSFT`**, **`cpu_gcc.s | GCC`** | CPU probe asm split |

#### B. PyMod / stock C — UEFI vs desktop Windows (`UEFI_C_SOURCE` / no CRT)

| Area | Files | Fix |
|------|--------|-----|
| POSIX / time | `posixmodule.c`, `timemodule.c` | No **`malloc.h`**; tz stubs on UEFI |
| Pickle / faulthandler | `_pickle.c`, `faulthandler.c` | MSVC opcode macros; no **`_set_abort_behavior`** on UEFI |
| OpenSSL async / threads / capi | `async_win.h`, `async_local.h`, `threads_win.c`, `e_capi.c` | Skip **`windows.h`** when **`UEFI_C_SOURCE`** |
| Core math / I/O | `pycore_pymath.h`, `pyhash.c`, `mysnprintf.c`, `dynamic_annotations.c` | No x87 **`__control87_2`**; portable **`ROTATE`**; **`vsnprintf`**; empty Valgrind path on UEFI |
| EFI entry / exceptions | `edk2main.c`, `edk2excep.c`, `edk2stack.nasm`, `edk2handler.nasm`, `edk2asm.h`, `edk2main.h`, `edk2excep.h`, `edk2stack.h` | NASM **`edk2_read_rsp`** / **`edk2_pause`**; no **`intrin.h`**; MSVC IDT layout; avoid **`Python.h`** in excep C file |
| **`pyconfig.h`** | PyMod + **`efi/Include/pyconfig.h`** | **`UEFI_MSVC_{32,64}`**, LLP64 **`SIZEOF_*`** (V2) |

#### C. deepfreeze ↔ global immortal strings

| Issue | Fix |
|-------|-----|
| MSVC **C2039** on **`_py_d`**, **`_py_dot`**, … | Ran **`Tools/build/generate_global_objects.py`** → updated **`pycore_global_strings.h`**, **`pycore_runtime_init_generated.h`**, **`pycore_unicodeobject_generated.h`**, fini header |
| Stale **`&_Py_STR(dot)`** (and **`percent`**, **`open_br`**, **`close_br`**) in **`deepfreeze.c`** | Replaced with **`_Py_SINGLETON(strings).ascii[N]`** (matches current **`deepfreeze.py`**) |
| **C4295** on **`co_code_adaptive`** | Fixed Session 7: brace initializers in **`deepfreeze.py`** + **`deepfreeze.c`** in repo (**`git add -f`**) |

#### D. OpenSSL link / MSFT intrinsics (UEFI has no desktop CRT)

| Symbol / topic | Fix |
|----------------|-----|
| **`_lrotl`**, **`_lrotr`**, **`_byteswap_*`** | **`openssl_uefi_msvc.c`** stubs; header guards on **`aes_local.h`**, **`cast_local.h`**, **`rc5_local.h`**, **`modes_local.h`** to use portable macros on **`UEFI_C_SOURCE`** |
| **`strerror_s`** | **`o_str.c`**: skip MSVC CRT path on UEFI; stub in **`openssl_uefi_msvc.c`** |
| **`ERR_load_DSO/UI`**, **`DSO_*`** | **`opensslconf.h`**: **`OPENSSL_NO_DSO`**, **`OPENSSL_NO_UI`**; **`err_all.c`** guards; minimal **`DSO_load`/`DSO_bind_func`/`DSO_free`** stubs |
| **`PyInit__ctypes_test`** | **`config.c`**: omit **`_ctypes_test`** from UEFI inittab (INF still **`| GCC`** only for test module) |

#### E. libffi MSFT

| Issue | Fix |
|-------|-----|
| Missing **`ffi_type_*`** | Add **`libffi_msvc/types.c | MSFT`** to INF |
| Missing **`ffi_prep_cif_var`** | Implement in **`libffi_msvc/prep_cif.c`**; declare in **`ffi.h`**; use **`FFI_BAD_TYPEDEF`** (legacy libffi has no **`FFI_BAD_ARGTYPE`**) |

#### F. GCC parity note

Same **`Python312.inf`** lists vendored **zlib**, **OpenSSL** (libcrypto + libssl), **`_hashopenssl.c`**, **`_ssl.c`**, **`Parser/myreadline.c`**. **readline** remains **pyreadline** staged by **`create_python_pkg.*`** (not a separate GNU readline build). **Mandatory:** re-run WSL **`BUILD_PYTHON312`** after this merge (not re-verified in Session 6 log).

### 2026-07-20 / 2026-07-21 — Session 7 (post-link — path, warnings, docs, hygiene)

**Branch tip (pushed):** `5624f81c` on **`origin/feature/python-3.12.13-vs2022`**.

| Commit | Summary |
|--------|---------|
| **`89fb9a0b`** | **UEFI path:** **`PREFIX`/`EXEC_PREFIX`** → **`\\EFI`**; **`getpath.py`** volume + relative prefix; **`PyMod/getpath.c`** **`real_executable`** from cwd + program name; **`uefipath.py`**; **`getpath.h`** force-added |
| **`9f89f39c`** | **`binascii.c`** C4245; **`deepfreeze.py`** + **`deepfreeze.c`** (**`co_code_adaptive`** brace inits, C4295) |
| **`83dc95d3`** | **Docs:** build guide, migration status, **`Python312_VS2022_GCC_Toolchain_Deviations.md`** |
| **`cc7800db`** | **`Lib/asyncio/uefi_events.py`** (mirrors PyMod; **`asyncio`** on UEFI) |
| **`5624f81c`** | **`.gitignore`:** **`myUEFIPy312/`**, stray top-level **`Modules/openssl|zlib|efi`** |

**Runtime / deploy (user):**

- **`create_python_pkg.bat VS2022 RELEASE X64`** — packaging **Done**; deploy to UEFI attempted.
- **Blocker:** VS2022 **`Python312.efi`** — blank KVM / no REPL (lab retest: **`-S`**, **`map -r`**, volume vs **`PREFIX`**, GCC A/B on same stick).

**Repo hygiene:** Reverted local **`StdLib/`** patch dirt and **`Python-3.6.8/`** overlay; removed broken **`LibOpenSSL/openssl`** junction ( **`git status`** warning on Windows). Working tree **clean**; re-**`git apply`** patches before next build.

### 2026-07-22 — Session 8 (MIN/FULL INF, VS2022 runtime bisect)

1. **`Python312_MIN.inf`** + **`Python312.inf`** — DSC selects module via **`BUILD_PYTHON312_FULL`** (no `!if` in INF).
2. **`msvc_chkstk.c | MSFT`** for MIN **`__chkstk`**; FULL uses **`libffi_msvc/ffi.c`**.
3. **`PY_UEFI_MSVC_368_ENTRY`** in **`edk2main.c`** — fixes hang inside **`ShellCEntryLib`** after stack switch on VS2022.
4. **`PY_UEFI_BOOT_TRACE`** — optional firmware **`Print`** ladder (**`edk2main.c`**, **`Main.c`**, **`python.c`**).
5. **deepfreeze / globals:** **`generate_global_objects.py`** (skip 1-char **`_Py_ID`**); **`fix_deepfreeze_latin1.py`**; **`AttributeError`** / **`statically_allocated`** stale **`deepfreeze.c`** diagnosed.

### 2026-07-22 — Session 9 (Windows frozen regen + MIN smoke sign-off)

1. **`Tools/build/regen_frozen_windows.cmd`** + docs (commits **`a0fb27fc`**, **`109f4a0a`**).
2. User ran regen with host **3.12.x**, rebuilt **MIN**, deployed — **all MIN smokes pass** (see **Current status** above).
3. **V6 MIN** closed; **V6 FULL** remains open.

---

## Phase V0 — Prerequisites and baseline capture

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V0.1 | Diff 3.6.8 VS2022 INF/pyconfig vs 3.12 | **Skipped** (use **`Python368.inf`** as live reference) |
| V0.2 | Record green **`BUILD_PYTHON368`** command | **Done** — see **V1.7** |
| V0.3 | Confirm GCC FULL baseline | **Done** — [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md) Phase **8** |

### Phase V0 result

**Skipped** — optional; 3.6.8 path used ad hoc.

---

## Phase V1 — Windows host and EDK workspace

**Exit criteria:** BaseTools built; `PACKAGES_PATH` / `EDK2_LIBC_PATH` documented; **`BUILD_PYTHON368`** smoke.

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V1.1 | Git, Python 3.10+, VS2022, NASM | **Done** |
| V1.2 | `edk2` + **`edk2-libc-jp-vsfix`** on **`feature/python-3.12.13-vs2022`** | **Done** |
| V1.3 | Apply **`patches/*.patch`** locally | **Done** (0001–0004) |
| V1.4 | **`srcprep.py`**; `PLATFORM "uefi"` | **Done** |
| V1.5 | Frozen + **`deepfreeze.c`** | **Done** (24× `.h` + deepfreeze present) |
| V1.6 | **`Edk2ToolsBuild.py -t VS2022`** | **Done** |
| V1.7 | **`BUILD_PYTHON368`** VS2022 X64 RELEASE | **Done** |
| V1.8 | Windows + 3.6.8 build guides | **Done** |

### V1.7 smoke command

```cmd
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix
set PACKAGES_PATH=c:\Users\njayapra\github\edk2;%EDK2_LIBC_PATH%
set NASM_PREFIX=C:\NASM\
cd /d c:\Users\njayapra\github\edk2
call edksetup.bat
build -t VS2022 -a X64 -b RELEASE -p AppPkg/AppPkg.dsc -D BUILD_PYTHON368
```

Expected artifact: `edk2\Build\AppPkg\RELEASE_VS2022\X64\Python.efi` (path may vary slightly).

### Phase V1 result

**Done.**

---

## Phase V2 — `pyconfig.h` / `UEFI_MSVC_*`

**Exit criteria:** Preprocessor sees correct **`SIZEOF_*`** and **`PLATFORM "uefi"`** under **`/DUEFI_MSVC_64`**.

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V2.1 | Port **`UEFI_MSVC_*`** / **`_MSC_VER`** blocks in **`PyMod-3.12.13/Include/pyconfig.h`** | **Done** |
| V2.2 | Mirror in **`PyMod-3.12.13/efi/Include/pyconfig.h`** | **Done** |
| V2.3 | Run **`srcprep.py`** | **Done** (repeat after edits) |
| V2.4 | GCC **`#else`** unchanged | **Done** — **`verify_pyconfig_gcc.sh`** |
| V2.5 | MSVC proof under **`/DUEFI_MSVC_64`** | **Done** — **`verify_pyconfig_msft.bat`** |

### Proof (re-run after any pyconfig change)

```cmd
cd AppPkg\Applications\Python\Python-3.12.13
python srcprep.py
cd vs2022_verify
call edksetup.bat   REM from edk2 WORKSPACE, or use VS2022 x64 tools shell
verify_pyconfig_msft.bat
```

WSL: **`vs2022_verify/verify_pyconfig_gcc.sh`**

Harness: [`Python-3.12.13/vs2022_verify/README.txt`](Python-3.12.13/vs2022_verify/README.txt)

### Phase V2 result

**Done** (2026-07-20).

---

## Phase V3 — `Python312.inf` MSFT `[BuildOptions]`

**Exit criteria:** First **`build -t VS2022`** gets **past compiling early C files** (link may fail).

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V3.1 | **`MSFT:*_*_*_CC_FLAGS`** (`/GL-` `/Oi-`, includes, UEFI defines, `/wd…`) | **Done** |
| V3.2 | **`[BuildOptions.X64]`** **`/DUEFI_MSVC_64`** | **Done** |
| V3.3 | **`[BuildOptions.IA32]`** **`/DUEFI_MSVC_32`** (defer IA32 build) | **Done** |
| V3.4 | Keep **`GCC:*_*_*_CC_FLAGS`** unchanged | **Done** |
| V3.5 | Run **`build -t VS2022 -D BUILD_PYTHON312`**; save log | **Done** (Session 6) |

### Target command (V3 gate)

```cmd
build -t VS2022 -a X64 -b RELEASE -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312
```

(Same env as **V1.7**; run from **`edk2`** WORKSPACE.)

### Phase V3 result

**Done** (2026-07-20, Session 6).

---

## Phase V4 — Toolchain splits + MIN VS2022 link

**Exit criteria:** **`Python312.efi`** links for MIN (or first FULL link if not using MIN INF variant).

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V4.1 | **`Python/asm_trampoline.S | GCC`** | **Done** |
| V4.2 | Vendored **libffi `.S` | GCC**; **libffi `.c` | GCC** | **Done** |
| V4.3 | **`libffi_msvc`** + **`| MSFT`** **`_ctypes`** (3.6.8 layout) | **Done** (copied from 3.6.8) |
| V4.4 | **`[Sources.X64]`** `win64.asm`, **`cpu.nasm`**, **`cpu_gcc.s`** | **Done** |
| V4.5 | PyMod **`UEFI_MSVC_*`** / UEFI guards in `.c` / headers | **Done** (Session 6 — see work log **§B–E**) |
| V4.6 | **`BUILD_PYTHON312 -t VS2022`** compile + link | **Done** (Session 6) |
| V4.7 | GCC regression after INF edits | **Pending** (re-run WSL **`BUILD_PYTHON312`** + **`verify_pyconfig_gcc.sh`**) |

### Files added for MSFT ctypes (Session 5)

```text
Python-3.12.13/Modules/_ctypes/libffi_msvc/     (from 3.6.8)
PyMod-3.12.13/Modules/_ctypes/libffi_msvc/
PyMod-3.12.13/Modules/_ctypes/malloc_closure.c
PyMod-3.12.13/Modules/cpu.nasm, cpu_gcc.s, cpu_ia32.nasm, cpu_ia32_gcc.s
```

### Phase V4 result

**Done** — FULL monolithic link on VS2022 X64 RELEASE (Session 6). Runtime smoke (**V6**) not yet run.

---

## Phase V5 — Packaging on Windows

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V5.1 | **`create_python_pkg.bat VS2022 RELEASE X64 <OutFolder>`** | **Done** (user; use **jp** fork script, not stub **`edk2-libc`**) |
| V5.2 | Layout **`EFI\bin`**, **`EFI\lib\python3.12`**, empty **`lib-dynload`** | **Done** |
| V5.3 | **`PREFIX`** matches deploy volume (**`fsN:\EFI`**, not fixed **`fs0:`**) | **Done** (Session 7 getpath + pyconfig) |

### Phase V5 result

**Done** (packaging exercised on Windows). Redeploy after rebuilding **`Python312.efi`** post-Session 7 commits.

---

## Phase V6 — Runtime smoke (MIN → FULL)

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V6.1 | Banner **3.12.13**, `import os, sys, json` | **Blocked** (no REPL on first VS2022 deploy — KVM) |
| V6.2 | FULL: `zlib`, `readline`, `ctypes`, `hashlib`, `ssl` | **Not started** |
| V6.3 | **`Python312.efi -S`** / **`-v`** from Shell on correct **`fsN:`** | **Pending** (lab) |

See port plan **§ Phase V6** for full matrix.

### Phase V6 result

**MIN (VS2022):** **Done** (2026-07-23) — **`-h`**, **`-S -c`**, default / **`-S`** REPL, **`exit(0)`** → Shell → **`exit`** → firmware. Production REPL uses **stdio TTY** (pyreadline off by default); see **Runtime Notes §10**. **FULL** UEFI smoke still open.

---

## Phase V7 — Docs and CI

### Checklist

| Step | Action | Result |
|------|--------|--------|
| V7.1 | [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) | **Done** |
| V7.2 | [`Python368_Windows_VS2022_Build_Guide.md`](./Python368_Windows_VS2022_Build_Guide.md) | **Done** |
| V7.3 | This status doc (GCC-style) | **Done** (Session 7 refresh) |
| V7.3b | [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) | **Done** |
| V7.4 | **`Py312ReadMe.txt`** VS2022 section | **Not started** |
| V7.5 | **`build-python312-uefi-vs2022.yaml`** | **Not started** |

### Phase V7 result

**Partial.**

---

## Phase V8 — Vendored FULL on VS2022

Execute in order **8.1 → 8.2 → 8.5 → 8.3 → 8.4** (same as GCC); after each batch: **`build -t VS2022`** + smoke.

| Step | GCC status (reference) | VS2022 |
|------|------------------------|--------|
| 8.1 zlib | **Done** on GCC | **Done** (shared INF sources) |
| 8.2 readline | **Done** on GCC | **Done** ( **`myreadline.c`** + package staging) |
| 8.5 ctypes / libffi | **Done** on GCC | **Done** (**`libffi_msvc | MSFT`**) |
| 8.3 hashlib | **Done** on GCC | **Done** (shared OpenSSL libcrypto) |
| 8.4 ssl | **Done** on GCC | **Done** (shared libssl + **`_ssl.c`**) |

### Phase V8 result

**Done** on MSVC build (Session 6) — same vendored trees as GCC; MSFT-only **`openssl_uefi_msvc.c`** and **`libffi_msvc`** backend.

---

## GCC regression gate (mandatory after VS2022 tree changes)

After **`Python312.inf`**, **`pyconfig.h`**, or PyMod MSVC edits:

1. WSL: **`git apply`** patches if needed → **`srcprep.py`** → **`BUILD_PYTHON312 -t GCC`** → **`create_python_pkg.sh`**
2. **`vs2022_verify/verify_pyconfig_gcc.sh`**
3. On Windows (after pyconfig edits): **`vs2022_verify/verify_pyconfig_msft.bat`**

Last known green GCC: user WSL **2026-07-20** (after V2/V3 INF prep). **Re-run required** after Session 6 (deepfreeze, generated headers, OpenSSL **`opensslconf.h`**, **`config.c`**).

---

## Known issues / follow-ups

1. ~~**V6 MIN runtime smoke**~~ — **Done** on VS2022 (2026-07-22): **`-h`**, **`-S -c "import sys; print(sys.version)"`**, **`-S -c "print('ok')"`**, interactive REPL. **FULL** runtime + Phase 8 import smokes still open.
2. **WSL GCC regression** not re-run after Session 6–7 — mandatory before merge to **`apppkg`**.
3. Re-**`git apply`** **`patches/*.patch`** after **`StdLib/`** cleanup (patches are not committed).
4. **`_ctypes_test`**: compiled **`| GCC`** only; excluded from UEFI **`config.c`** on both toolchains.
5. Do not put port tools under CPython **`Tools/`** — use **`vs2022_verify/`**.
6. **`build-python312-uefi-vs2022.yaml`** (**V7.5**) not added.
7. **`deepfreeze.c`** / **`getpath.h`**: tracked via **`git add -f`** (CPython **`.gitignore`**); regenerate when changing **`getpath.py`** / deepfreeze inputs.

---

## Next actions (recommended)

**Follow:** [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) · [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10

On Windows, in order:

1. **Commit/push** remaining **`edk2-libc-jp-vsfix`** changes (MIN INF, 368 entry, regen outputs, StdLib probes if kept) — local tree still has uncommitted runtime work beyond **`regen_frozen_windows.cmd`** commits.
2. **FULL** build: **`BUILD_PYTHON312_FULL=TRUE`**, same **`PY_UEFI_MSVC_368_ENTRY`** on **`Python312.inf`**, package, deploy; smoke **`import zlib`**, **`import ssl`**, **`import ctypes`**, **`hashlib`** (runtime notes §10).
3. **Cleanup (optional):** remove **`PY_UEFI_BOOT_TRACE`** / **`Main.c`** probes when FULL is stable; consider **`#undef Py_DEBUG`** in UEFI **`pyconfig.h`**.
4. WSL: **`BUILD_PYTHON312 -t GCC`** + **`create_python_pkg.sh`** after deepfreeze/header churn (regression gate).
5. **V7:** **`build-python312-uefi-vs2022.yaml`**, **`Py312ReadMe.txt`** VS2022 section.

---

## Locked policy — StdLib patches

Same as GCC AppPkg status:

- **Do not commit** applied **`StdLib/`** / **`StdLibPrivateInternalFiles/`** on this branch.
- **Required** before build:  
  `git apply --ignore-whitespace AppPkg/Applications/Python/Python-3.12.13/patches/*.patch`
- MSVC-safe fixes for patched StdLib: commit **`patches/0001`**, **`patches/0002`** only.

---

## Reference

| Item | Path |
|------|------|
| VS2022 port plan | `AppPkg/Applications/Python/Python312_VS2022_Port_Plan.md` |
| **GCC vs VS2022 deviations** | `AppPkg/Applications/Python/Python312_VS2022_GCC_Toolchain_Deviations.md` |
| V2 proof | `AppPkg/Applications/Python/Python-3.12.13/vs2022_verify/` |
| Monolithic INF | `AppPkg/Applications/Python/Python-3.12.13/Python312.inf` |
| 3.6.8 MSVC INF | `AppPkg/Applications/Python/Python-3.6.8/Python368.inf` |
| GCC AppPkg status | `AppPkg/Applications/Python/Python312_AppPkg_Migration_Status.md` |
| WSL GCC guide | `AppPkg/Applications/Python/Python312_WSL_GCC_Build_Guide.md` |
