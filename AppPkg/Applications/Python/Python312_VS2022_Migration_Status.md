# Python 3.12.13 AppPkg — VS2022 / Windows Migration Status

**Plan:** [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md)  
**Windows build guide:** [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md)  
**MIN build (two INF):** [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md)  
**UEFI runtime / VS2022 fixes:** [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md)  
**GCC vs VS2022 deviations:** [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md)  
**Lab sign-offs / debug:** [`Python312_VS2022_Lab/`](./Python312_VS2022_Lab/)  
**3.6.8 VS2022 walkthrough:** [`Python368_Windows_VS2022_Build_Guide.md`](./Python368_Windows_VS2022_Build_Guide.md)  
**GCC reference (FULL port):** [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md)  
**GCC regression build:** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)  
**Started:** 2026-07-18  
**Updated:** 2026-08-26 (FULL **`import ssl`** Shell **`exit`** — lab sign-off in [`Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md`](./Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md))
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

## Current status (2026-07-23)

**Branch:** `feature/python-3.12.13-vs2022` · **Workspace clone:** `edk2-libc-jp-vsfix` · **Remote:** `jpshivakavi/edk2-libc-jp`  
**Branch tip (pushed):** **`3568d02d`** — VS2022 FULL **`import ssl`** Shell **`exit`** fix; see tag **`python312-vs2022-full-lab-2026-08-26`** below.

---

## Git tags (VS2022 track)

| Tag | Commit | Meaning |
|-----|--------|---------|
| **`python312-vs2022-full-lab-2026-08-26`** | **`3568d02d`** | **VS2022 FULL UEFI lab sign-off (2026-08-26):** `import sys` / **`import ssl`** / **`ssl.create_default_context()`** / **hashlib** / **ctypes** one-liners; **`Shell>`** → **`exit`** → BIOS/setup. PyMod **`Lib/ssl/`** (`_uefi_min`), MSVC teardown parity, post-finalize OpenSSL/console handoff. Details: [`Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md`](./Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md). |

**Checkout code at tag:** `git fetch origin tag python312-vs2022-full-lab-2026-08-26 && git checkout python312-vs2022-full-lab-2026-08-26`

**After this tag (local WIP, not in tag):** realign stock **`Python-3.12.13/`** with upstream CPython + consolidate UEFI deltas under **`PyMod-3.12.13/`** only — build/deploy/verify before committing that follow-up.

**GCC AppPkg milestone tags (reference):** `python312-apppkg-8.2` … `8.5` on `feature/python-3.12.13-apppkg` — see [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md).

---

| Area | State |
|------|--------|
| **VS2022 MIN** (`Python312_MIN.inf`, default DSC) | **Build + manufacturing UEFI runtime Done** — **`-h`**, **`-S -c`**, default / **`-S`** REPL, **`exit(0)`** → Shell → **`exit`** → firmware; **`import readline`** without env stays stub-safe |
| **MSVC entry** | **`PY_UEFI_MSVC_368_ENTRY=1`** — **`ShellCEntryLib`** on Shell stack (no custom stack switch / IDT); **GCC still uses** **`edk2_switch_stack` + `py_install_idt`** (see deviations §11) |
| **REPL / readline vs GCC** | **Same Python sources** on branch disable pyreadline by default on **`os.name == 'uefi'`**; **GCC Phase 8** historically ran **pyreadline + Tab** without Shell hang; **VS2022 required** this policy for sign-off — treat **VS2022** as **stdio REPL + optional `PY_UEFI_READLINE=1`** only |
| **Frozen / deepfreeze** | **`Python/deepfreeze/deepfreeze.c`** is **committed** (latin1 + **`statically_allocated`** fixes applied). Regen only when changing frozen inputs — use **`regen_frozen_windows.cmd`** (§5): **`deepfreeze.py`** → **`fix_deepfreeze_statically_allocated.py`** → **`generate_global_objects.py`** → **`fix_deepfreeze_latin1.py`** |
| **VS2022 FULL** (`BUILD_PYTHON312_FULL=TRUE`, `Python312.inf`) | **Link Done** (Session 6); **368 entry on FULL INF**; **Lab (2026-08-26):** `import ssl` / hashlib / ctypes one-liners + Shell **`exit`** → BIOS — see [`Python312_VS2022_Lab/`](./Python312_VS2022_Lab/) |
| **GCC regression** | Last green **2026-07-20**; **mandatory re-run** after commits **`59000200`** / **`3814cf9a`** (REPL teardown, `readline.py`, `pylifecycle.c`) |
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
| V6 | Runtime smoke (MIN → FULL) | **Partial (lab)** — MIN Session 10; **FULL:** core Phase 8 one-liners + Shell **`exit`** signed off 2026-08-26 ([lab note](./Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md)); extended matrix (REPL, zlib, readline) open |
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
| **`Python-3.12.13/` tree** | Must match **upstream CPython 3.12.13** for forked paths; **do not commit** `Lib/ssl/` or other srcprep overlays under stock tree — only **`PyMod-3.12.13/`** (see **`Tools/restore_upstream_from_cpython.py`**) |
| MSVC sizing | **`/DUEFI_MSVC_64`** on X64; **`_MSC_VER`** LLP64 (`SIZEOF_LONG` 4); GCC **`#else`** LP64 (8) |
| StdLib patches | Apply **`patches/*.patch`** locally; **do not commit** `StdLib/` on branch |
| **`PREFIX` / getpath** | **`\\EFI`** relative to interpreter volume **`fsN:`** (3.6.8-style); not hard-coded **`fs0:`** — see Session 7 |
| ctypes on MSFT | **`libffi_msvc`** + **`| MSFT`** `_ctypes` sources (3.6.8 pattern); vendored **libffi `.S`** **`| GCC`** only |
| First MSVC target | **X64 RELEASE** (IA32 deferred) |
| Proof before large builds | **`vs2022_verify/`** after every **`pyconfig.h`** change |
| UEFI **`import ssl`** | **`PyMod-3.12.13/Lib/ssl/`** — **`_uefi_min.py`** at import (VS2022 Shell **`exit`**); not stock **`ssl.py`** — see **§ UEFI ssl scope** below |

---

## UEFI `ssl` module (FULL) — scope and limitations

On **`os.name == 'uefi'`**, **`import ssl`** loads **`ssl._uefi_min`** only ([`PyMod-3.12.13/Lib/ssl/__init__.py`](./Python-3.12.13/PyMod-3.12.13/Lib/ssl/__init__.py)). This is **not** CPython 3.12’s full pure-Python **`ssl`** package ([`_stdlib.py`](./Python-3.12.13/PyMod-3.12.13/Lib/ssl/_stdlib.py) is used on desktop builds only). The C extension **`_ssl`** (OpenSSL **libssl**) is still linked in FULL builds; limits are in the **Python wrapper**, not “no TLS in firmware.”

### Provided (manufacturing / smoke / typical bootstrap)

| Area | UEFI behavior |
|------|----------------|
| **`import ssl`**, **`import _ssl`** | OK; Shell **`exit`** signed off with minimal wrapper (lab 2026-08-26). |
| **Exceptions, version constants** | Re-exported from **`_ssl`** (`SSLError`, `OPENSSL_VERSION_*`, protocol/cert constants, `RAND_*`, feature flags). |
| **`SSLContext`** | Alias of **`_ssl._SSLContext`** (C type) — cert chains, verify mode, cipher/options APIs on the context object itself. |
| **`create_default_context`**, **`_create_unverified_context`** | Implemented; **`Purpose`** is a simple OID holder (strings), not **`_ASN1Object`**. |
| **`http.client` / stdlib HTTPS hook** | **`_create_default_https_context`** points at **`create_default_context`**. |
| **System CA store** | **`load_default_certs`** is **not** called on UEFI (no OS trust store); pass **`cafile`/`capath`/`cadata`** explicitly. |
| **Low-level TLS on a socket** | C **`_SSLContext._wrap_socket(...)`** exists in **`_ssl`**; there is no Python **`SSLSocket`** subclass in **`_uefi_min`**. |

### Not provided at `import ssl` (use `_ssl` or extend PyMod deliberately)

| Missing vs stock **`ssl`** | Impact |
|-----------------------------|--------|
| **`SSLSocket`**, **`SSLObject`** Python classes | No **`context.wrap_socket()`** / **`wrap_bio()`** helpers from the stdlib subclass; no **`get_server_certificate()`** (depends on **`SSLSocket`** + **`create_connection`**). |
| **IntEnum/IntFlag mirrors** (`VerifyMode`, `Options`, `_SSLMethod`, `AlertDescription`, …) | Names not auto-registered on **`ssl`**; use **`_ssl.PROTOCOL_*`**, **`_ssl.CERT_*`**, etc., or import from **`_ssl`**. |
| **`Purpose` / `_ASN1Object`** with **`fromname`/`fromnid`** | No import-time **`OBJ_txt2obj`**; OIDs are plain strings on UEFI. |
| **Cert utilities** | No **`cert_time_to_seconds`**, **`DER_cert_to_PEM_cert`**, **`PEM_cert_to_DER_cert`**, **`match_hostname`** helpers in the minimal module (remain in **`_stdlib.py`** only). |
| **`get_default_verify_paths`**, Windows **`enum_certificates`** | Not exported on UEFI minimal **`ssl`**. |
| **`import socket` via `ssl`** | Minimal module does **not** pull **`Lib/socket.py`** at import (by design). |

### Expectations for porting apps

- **Manufacturing smokes** (`import ssl`, **`create_default_context()`**, hashlib, ctypes): **in scope** and lab-signed-off.
- **Full desktop-style HTTPS clients** (urllib without extra work, **`ssl.get_server_certificate`**, rich enum surface): **out of scope** until a **lazy** or **explicit** load of selected **`_stdlib.py`** pieces is designed and re-tested for Shell **`exit`** on VS2022 (and GCC).
- **Preferred extension path:** add targeted APIs to **`_uefi_min.py`** or lazy-import from **`_stdlib.py`** behind functions, **not** restoring monolithic import of full **`ssl.py`** on UEFI.

Details and deploy checklist: [`Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md`](./Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md).

---

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

### 2026-07-23 — Session 10 (VS2022 REPL exit, Shell teardown, readline stub — **user-verified**, pushed)

**Commits on `origin/feature/python-3.12.13-vs2022`:**

| Commit | Summary |
|--------|---------|
| **`bdb1033c`** | VS2022 MIN runtime: **368 entry**, frozen regen tooling, smoke green baseline |
| **`59000200`** | Fix VS2022 UEFI REPL **`exit()`** and Shell teardown hangs — stdio REPL default; **`site.py`** skip **`enablerlcompleter`** on UEFI; **`edk2console`** detach/drain/**`CloseProtocol`**; **`Py_FinalizeEx`** UEFI skips; **`PY_UEFI_MSVC_368_ENTRY`**, boot traces |
| **`3814cf9a`** | **`readline.py`** UEFI **stub** unless shell env **`PY_UEFI_READLINE=1`**; lazy **ConIn** **`OpenProtocol`** + **`CloseProtocol`** on detach; docs §10 |

**User-verified (VS2022 MIN + 368 entry, stick with updated `.efi` + `EFI\lib\python3.12\`):**

- **`Python312.efi`** / **`-S`**: interactive REPL → **`exit(0)`** → Shell → **`exit`** → firmware OK
- **`import readline`** without **`PY_UEFI_READLINE=1`**: stub only; Shell **`exit`** still OK
- **`-S -I`**: stdio REPL (no site hook); same teardown success

**Not signed off:** default **pyreadline** line editing on VS2022 (experimental path needs **`PY_UEFI_READLINE=1`** + optional compile **`PY_UEFI_PYREADLINE`**). A local “re-enable pyreadline by default” experiment was **reverted** before push — not on the branch.

**GCC note:** Session 10 changes are mostly **`UEFI_C_SOURCE`** / **`os.name == 'uefi'`** (both toolchains when AppPkg defines UEFI). **Observed behavior still diverges:** GCC reference smoke used **pyreadline** successfully; VS2022 manufacturing uses **stdio REPL**. Re-run WSL **`BUILD_PYTHON312`** + package smoke after pull — see [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) **§11**.

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
| V6.1 | Banner **3.12.13**, `import os, sys, json` | **Done** (MIN, Session 9–10) |
| V6.2 | FULL: `zlib`, `readline`, `ctypes`, `hashlib`, `ssl` | **Not started** |
| V6.3 | **`Python312.efi -S`** / **`-v`** from Shell on correct **`fsN:`** | **Done** (MIN; Session 10) |

See port plan **§ Phase V6** for full matrix.

### V6 smoke commands (UEFI Shell)

**Deploy first:** [`create_python_pkg.bat`](./Python-3.12.13/create_python_pkg.bat) from **`Python-3.12.13/`** ( **`WORKSPACE`**, **`EDK2_LIBC_PATH=edk2-libc-jp-vsfix`** ); copy **`<OutFolder>\EFI`** to **`fsN:\EFI`**; **`map -r`**, **`fsN:`**, **`cd EFI\bin`**. Packaging: [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) §7 (deploy table) · runtime notes §8.

**Requires** **`PY_UEFI_MSVC_368_ENTRY=1`** on MSFT flags (**`Python312_MIN.inf`** / **`Python312.inf`**). Default manufacturing REPL: stdio (no pyreadline); runtime notes §10.

#### MIN — signed off (2026-07-23, Session 10)

Run on **`Python312.efi`** built from default DSC (no **`BUILD_PYTHON312_FULL`**):

```text
Python312.efi -h
Python312.efi -S -c "import sys; print(sys.version)"
Python312.efi -S -c "print(1+1)"
Python312.efi -S -c "import os, sys, json; print('ok')"
Python312.efi
Python312.efi -S
Python312.efi -S -I
```

| Check | Expected |
|--------|----------|
| **`-h`** | Help text; return to Shell prompt |
| **`-S -c`** | **3.12.13** banner / output; return to prompt |
| **REPL** (default, **`-S`**, **`-S -I`**) | **`>>>`** via stdio; **`exit(0)`** returns to Shell |
| **Teardown** | After REPL: Shell **`exit`** → firmware/setup (no hang) |
| **Relaunch** | **`Python312.efi`** again shows banner |
| **`import readline`** (in REPL, no env) | Stub only; Shell **`exit`** still OK |
| **`import ssl`** / **`import ctypes`** | **Fail** (MIN has no Phase 8) |
| **`import hashlib`**, **`import os`** | **OK** |

#### FULL — pending sign-off (after **`2190f54c`**: 368 entry on **`Python312.inf`**, latin1 **`deepfreeze.c`**)

Build: **`build … -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE`**. Repackage so **`EFI\bin\Python312.efi`** is from **`…\Python312\DEBUG\`** (FULL), not MIN.

Repeat **all MIN rows** above, then:

```text
Python312.efi -S -c "import zlib; print(zlib.__name__)"
Python312.efi -S -c "import ctypes; print(ctypes.__name__)"
Python312.efi -S -c "import hashlib; print(hashlib.__name__)"
Python312.efi -S -c "import ssl; print(ssl.__name__)"
Python312.efi -S -c "import zlib, ssl, ctypes, hashlib; print('phase8 ok')"
```

| Check | Expected |
|--------|----------|
| Phase 8 **`-S -c`** imports | **OK** (no hang, no silent exit) |
| **REPL → `exit(0)` → Shell `exit` → relaunch** | Same as MIN (no regression) |
| **`import readline`** without **`PY_UEFI_READLINE=1`** | Stub; teardown still OK |

Record FULL results here and in **Phase V6 result** when lab sign-off is done. Detailed order: [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §11.

**Not in manufacturing matrix:** **`PY_UEFI_READLINE=1`** / pyreadline line editing (experimental; runtime notes §10).

### Phase V6 result

**MIN (VS2022):** Session 10 (2026-07-23) — **`-h`**, REPL, stub **`import readline`** signed off. **WIP (2026-08):** Shell **`exit`** after **`-S -c`** (e.g. **`import sys`**) still hangs on lab hardware; use boot trace §7 playbook, stay on WIP **`edk2main`** handoff + FULL teardown skips until last line + **`exit`** behavior are recorded. **FULL** Phase 8 import smokes still open.

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

1. ~~**V6 MIN runtime smoke**~~ — **Done** on VS2022 (2026-07-23, Session 10): REPL + Shell **`exit`** + stub **`import readline`**. **FULL** runtime + Phase 8 import smokes still open.
2. **WSL GCC regression** not re-run after Session 6–10 — mandatory before merge to **`apppkg`** (REPL/readline policy may change GCC UX vs pre-**`59000200`** stick).
3. **VS2022 vs GCC runtime** is **not** identical for firmware entry and interactive REPL — documented in **GCC deviations §11**; do not assume GCC pyreadline behavior applies to VS2022 manufacturing.
4. Re-**`git apply`** **`patches/*.patch`** after **`StdLib/`** cleanup (patches are not committed).
5. **`_ctypes_test`**: compiled **`| GCC`** only; excluded from UEFI **`config.c`** on both toolchains.
6. Do not put port tools under CPython **`Tools/`** — use **`vs2022_verify/`**.
7. **`build-python312-uefi-vs2022.yaml`** (**V7.5**) not added.
8. **`deepfreeze.c`** / **`getpath.h`**: tracked via **`git add -f`** (CPython **`.gitignore`**); regenerate when changing **`getpath.py`** / deepfreeze inputs.

---

## Next actions (recommended)

**Follow:** [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) · [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10

On Windows, in order:

1. **FULL** build: **`BUILD_PYTHON312_FULL=TRUE`**, package, deploy; smoke **`import zlib`**, **`import ssl`**, **`import ctypes`**, **`hashlib`** (runtime notes §11). **`Python312.inf`** MSFT flags include **`PY_UEFI_MSVC_368_ENTRY`** (same as MIN).
2. **Cleanup (optional):** remove **`PY_UEFI_BOOT_TRACE`** / **`Main.c`** probes when FULL is stable; consider **`#undef Py_DEBUG`** in UEFI **`pyconfig.h`**.
3. WSL: **`BUILD_PYTHON312 -t GCC`** + **`create_python_pkg.sh`** — confirm REPL/Shell **`exit`** with post–Session 10 **`readline.py`** / **`site.py`** (deviations §11).
4. **V7:** **`build-python312-uefi-vs2022.yaml`**, **`Py312ReadMe.txt`** VS2022 section.
5. **Future (not manufacturing):** VS2022 default **pyreadline** parity with historical GCC smoke — needs dedicated branch + full teardown matrix.

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
