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
**Updated:** 2026-09-04 (PyMod frozen artifacts in git; fresh-clone build docs)  
**Strategy:** **Single line:** **`feature/python-3.12.13-vs2022`** for **`build -t GCC`** and **`-t VS2022`**. **`feature/python-3.12.13-apppkg`** kept as **read-only reference** (GCC port / 3.6.8 AppPkg structure alignment) — **no merge back into apppkg**. Same `PACKAGES_PATH=<edk2>;<edk2-libc>`; vendored libs in **`PyMod-3.12.13/Modules/`**  
**Branch:** **`feature/python-3.12.13-vs2022`** — sole manufacturing line (forked from **`feature/python-3.12.13-apppkg`**; apppkg now **reference only**)  
**Target repo:** `jpshivakavi/edk2-libc-jp` (push from **`edk2-libc-jp-vsfix`** when ready)  
**Windows WORKSPACE:** `c:\Users\njayapra\github\edk2` (tianocore/edk2 — `edksetup.bat`, `Build\`)  
**Libc clone / `EDK2_LIBC_PATH`:** `c:\Users\njayapra\github\edk2-libc-jp-vsfix` (active VS2022 workspace; branch **`feature/python-3.12.13-vs2022`**)  
**WSL GCC regression:** **2026-09-01** — FULL on **`feature/python-3.12.13-vs2022`** @ **`dbc8416c`** (edk2-py312 layout); Phase 8 **`-S -c`** matrix + Shell **`exit`** — [`Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md). Prior: **2026-07-20** MIN/prep only.  
**MSVC reference INF:** [`Python-3.6.8/Python368.inf`](./Python-3.6.8/Python368.inf)  
**3.6.8 VS2022 CI:** [`.github/workflows/build-python-uefi-vs2022.yaml`](../.github/workflows/build-python-uefi-vs2022.yaml) (`BUILD_PYTHON368` only today)

Build gate: **`-p AppPkg/AppPkg.dsc`** with `PACKAGES_PATH` including the libc fork — **not** `-p %EDK2_LIBC_PATH%\AppPkg\AppPkg.dsc` alone.

---

## Build workflows — fresh clone vs existing clone

**Branch:** **`feature/python-3.12.13-vs2022`** · **`EDK2_LIBC_PATH`** → **`edk2-libc-jp-vsfix`** (or equivalent fork path).

Frozen / deepfreeze outputs live under **`PyMod-3.12.13/Python/`** (`frozen_modules/*.h`, **`frozen.c`**, **`deepfreeze/deepfreeze.c`**) and are **committed** (since **`55219522`**). Stock **`Python/frozen_modules/`** is not used for builds.

| Situation | Steps |
|-----------|--------|
| **Fresh clone** | Clone libc fork → checkout **`feature/python-3.12.13-vs2022`** → libc patches if needed (often skip — **§ Branch drift**) → **`srcprep.py`** → **`build -D BUILD_PYTHON312`** (+ **`BUILD_PYTHON312_FULL=TRUE`** for FULL) → package script. **No** `make frozen` / **`regen_frozen_windows.cmd`** for first build. |
| **Existing clone (`git pull`)** | Pull → **`srcprep.py`** if overlay / **`srcprep.py`** changed → rebuild. Regen frozen **only** when editing frozen **`.py`** or refreshing deepfreeze — **`Tools/build/regen_frozen_windows.cmd`** (Windows). Delete stale **`Python/frozen_modules/*.h`** if upgrading from pre-PyMod layout. |
| **GCC (WSL)** | [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md) §6 |
| **VS2022 (Windows)** | [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) §6 |
| **Regen pitfalls** | [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §5 |

---

## Current status (2026-09-01)

**Branch:** `feature/python-3.12.13-vs2022` · **Workspace clone:** `edk2-libc-jp-vsfix` / WSL **`edk2-libc-jp-vsfix`** · **Remote:** `jpshivakavi/edk2-libc-jp`  
**Branch tip:** **`dbc8416c`** — GCC **`py312_boot_print_ascii`** fix in **`edk2main.c`**.

**Manufacturing sign-off (same branch):**

| Toolchain | Lab | Commit (reference) | Phase 8 **`-S -c`** + Shell **`exit`** | FULL stdio **`Python312.efi -S`** → **`exit(0)`** → Shell **`exit`** |
|-----------|-----|-------------------|----------------------------------------|-----------------------------------------------------------------------------|
| **VS2022 FULL** | 2026-08-26 / 27; REPL 2026-09-01 | **`4dec4edf`** (ssl/RNG), **`3568d02d`** (import ssl) | **Done** — [`Python312_VS2022_Lab/`](./Python312_VS2022_Lab/) | **Done** (2026-09-01) — no **`PY_UEFI_READLINE`** |
| **GCC FULL** | 2026-09-01 | **`dbc8416c`** | **Done** — Phase 8 matrix + optional **pyreadline** (**§ UEFI REPL / pyreadline**) | **Done** (2026-09-01) — no **`PY_UEFI_READLINE`** |

**Single codebase validated:** **`feature/python-3.12.13-vs2022`** builds **GCC** and **VS2022** FULL; shared PyMod (ssl, OpenSSL, teardown) did not regress GCC on hardware.

**Long-term build line:** **§ Single codebase — one branch for GCC and VS2022**.

### GCC FULL regression — build and smoke (2026-09-01)

**Environment:** WSL Ubuntu 20.04, **`build -t GCC -b NOOPT`**, **`BUILD_PYTHON312` + `BUILD_PYTHON312_FULL=TRUE`**, **`PACKAGES_PATH=$HOME/src/edk2-py312/edk2:$EDK2_LIBC_PATH`**, **`create_python_pkg.sh GCC NOOPT X64`**. Image uses GCC entry (**`edk2_switch_stack` + `py_install_idt`**), not MSVC 368 path. Canonical lab copy: [`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md).

#### Build notes (first GCC FULL on vs2022 tip)

| Issue | Fix |
|-------|-----|
| **`py312_boot_print_ascii` redefinition** | **`dbc8416c`**: implementation in **`edk2main.c`** only when **`PY_UEFI_BOOT_TRACE`** (MSFT-only in **`Python312.inf`**); GCC uses **`py312boot.h`** inline stub |
| **Missing `Python/frozen_modules/*.h`** | **Resolved (`55219522`)** — 24× headers + **`PyMod-3.12.13/Python/frozen.c`** committed under **`PyMod-3.12.13/Python/frozen_modules/`**; fresh clone needs no freeze step — [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md) §6 |

#### Manufacturing smoke (hardware — passed 2026-09-01)

Protocol: each **`Python312.efi -S -c "…"`** → **`Shell>`** → **`exit`** → BIOS/setup, **no hang**.

| Command | Result |
|---------|--------|
| `import sys; print('ok')` | OK |
| `import zlib; print(zlib.__name__)` | OK |
| `import hashlib; print(hashlib.sha256(b'x').hexdigest()[:8])` | OK |
| `import ctypes; print(ctypes.sizeof(ctypes.c_void_p))` | **`8`** (X64) |
| `import ssl; print(ssl.__file__)` | **`.../ssl/__init__.py`** (UEFI package) |
| `import ssl; ssl.create_default_context(); print('ok')` | OK |
| `import zlib, ssl, ctypes, hashlib; print('phase8 ok')` | OK |

#### Default stdio interactive REPL (hardware — passed 2026-09-01)

**Both** GCC and VS2022 **FULL** packages. **No** **`PY_UEFI_READLINE`**.

```text
Python312.efi -S
```

At **`>>>`**: trivial lines → **`exit(0)`** → **`Shell>`** → **`exit`** → BIOS/setup — **no hang**.

Optional **pyreadline** (env + **`import readline`**, U+001B pitfall, GCC-only opt-in lab): **§ UEFI REPL / pyreadline** below.

### Boot trace (GCC vs VS2022)

**`PY_UEFI_BOOT_TRACE=1`** is on **MSFT** `[BuildOptions]` only (**`Python312.inf`**). **GCC** images show **`Python312: UefiMain`**, **`Python312: enter main`**, and script output — **not** the MSVC finalize/ssl ladder unless **`-DPY_UEFI_BOOT_TRACE=1`** is added to **`GCC:*_*_*_CC_FLAGS`** for debug.

### UEFI interactive REPL and pyreadline (FULL)

**Manufacturing default (GCC and VS2022):** stdio **`>>>`** via StdLib TTY — **no** line editing, **no** Tab history, **no** **`edk2console`** until the operator opts in. This differs from **historical GCC AppPkg** lab notes where **pyreadline** was often on by default; **Session 10** policy on **`feature/python-3.12.13-vs2022`** applies to **both** toolchains in source.

**Packaging:** **`create_python_pkg.sh`** / **`.bat`** still stages **`EFI/lib/python3.12/readline.py`** and **`pyreadline/`** from **`PyMod-3.12.13/Modules/readline/`** (identical for GCC and VS2022 sticks).

#### How opt-in works (three layers)

| Layer | Default on UEFI | Effect |
|-------|-----------------|--------|
| **`PyMod-…/Modules/readline/readline.py`** | If **`os.name == 'uefi'`** and shell env **`PY_UEFI_READLINE`** is **not** `1`/`yes`/`true` → **stub** (no **`pyreadline`**, no **`edk2console`**) | **`import readline`** is safe for scripts; does not install **`PyOS_ReadlineFunctionPointer`** |
| **`PyMod-…/Modules/main.c` `pymain_import_readline`** | Under **`UEFI_C_SOURCE`**, **returns immediately** unless image built with **`PY_UEFI_PYREADLINE`** at **compile** time | **`Python312.efi -S`** does **not** auto-import readline (even when shell env is set) |
| **`PyMod-…/Lib/site.py` `enablerlcompleter`** | **`if os.name == 'uefi': return`** — no **`sys.__interactivehook__`** | Site does **not** auto-import readline on REPL start |

**Runtime enable (shell env):** before **`Python312.efi`**:

```text
set PY_UEFI_READLINE 1
```

This only allows the **real** **`readline.py`** (pyreadline path) when the module is **imported** — it does **not** wire the REPL by itself.

**Compile-time auto-import (optional rebuild):** add **`-DPY_UEFI_PYREADLINE=1`** to **GCC** or **MSFT** **`CC_FLAGS`** in **`Python312.inf`** so **`pymain_import_readline`** runs at startup (old “always on” feel). **Not** manufacturing default; re-smoke Shell **`exit`** after any change.

#### Operator workflow (pyreadline on GCC — lab 2026-09-01)

```text
set PY_UEFI_READLINE 1
Python312.efi -S
```

At **`>>>`**, run **`import readline` first** (before arrow keys or Tab). That calls **`console.install_readline`** → **`edk2console.install_readline_hook`** → **`PyOS_ReadlineFunctionPointer`**.

| Step | Expected |
|------|----------|
| Type a line, Enter | Accepted |
| Up-arrow | Recalls history |
| Tab | Completion (rlcompleter wired in **`readline.py`**) |
| **`exit()`** → **`Shell>`** → **`exit`** | Returns to BIOS — **no hang** (GCC FULL lab, 2026-09-01) |

#### Common mistake: arrow keys without `import readline`

If **`PY_UEFI_READLINE=1`** but **`import readline`** was **not** run (e.g. only **`import os`**), the REPL still uses **stdio** input. Arrow keys send **ANSI escape sequences**; Python may report:

```text
SyntaxError: invalid non-printable character U+001B
```

**`U+001B`** is **ESC** — not a broken build; import **`readline`** before using line-editing keys.

#### Lab status (hardware)

| Scenario | Toolchain tested | Result |
|----------|------------------|--------|
| Phase 8 **`-S -c`** (no pyreadline) | **GCC** + **VS2022** | **Pass** — Shell **`exit`** |
| Default **`Python312.efi -S`** (stdio REPL, no env) | **GCC FULL** + **VS2022 FULL** (2026-09-01) | **Pass** — trivial **`>>>`**, **`exit(0)`**, Shell **`exit`** |
| Default **`Python312.efi -S`** (stdio REPL) | **VS2022 MIN** (Session 10) | **Pass** |
| **`PY_UEFI_READLINE=1`** + **`import readline`** + history/Tab + teardown | **GCC only** (2026-09-01) | **Pass** |
| Same pyreadline opt-in on **VS2022** FULL | **Not re-smoked** on this branch after Session 10 | VS2022 historically hung Shell **`exit`** with pyreadline — see deviations **§11.3**, runtime notes **§10** |

**Takeaway:** **GCC** can use **optional pyreadline** with current teardown sources when env + **`import readline`** are used. **VS2022 manufacturing** stays **stdio default** (FULL **`-S`** signed off 2026-09-01); **VS2022 pyreadline** opt-in remains **not** re-lab’d.

**Cross-refs:** [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10 · [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) §11.3 · lab [`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md)

---

## Git tags (VS2022 track)

| Tag | Commit | Meaning |
|-----|--------|---------|
| **`python312-unified-full-lab-2026-09-01`** | **`753bfefb`** | **Unified FULL UEFI lab (2026-09-01):** **`feature/python-3.12.13-vs2022`** — **GCC** + **VS2022** FULL; Phase 8 **`-S -c`**, stdio **`Python312.efi -S`** → **`exit(0)`** → Shell **`exit`**; GCC optional **pyreadline**. Lab: [`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md). **V6 Done.** |
| **`python312-vs2022-full-lab-2026-08-26`** | **`3568d02d`** | **VS2022 FULL UEFI lab sign-off (2026-08-26):** `import sys` / **`import ssl`** / **`ssl.create_default_context()`** / **hashlib** / **ctypes** one-liners; **`Shell>`** → **`exit`** → BIOS/setup. PyMod **`Lib/ssl/`** (`_uefi_min`), MSVC teardown parity, post-finalize OpenSSL/console handoff. Details: [`Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md`](./Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md). |

**Checkout unified manufacturing pin:** `git fetch origin tag python312-unified-full-lab-2026-09-01 && git checkout python312-unified-full-lab-2026-09-01`

**Checkout VS2022-only pin (older):** `git fetch origin tag python312-vs2022-full-lab-2026-08-26 && git checkout python312-vs2022-full-lab-2026-08-26`

**GCC AppPkg milestone tags (reference):** `python312-apppkg-8.2` … `8.5` on `feature/python-3.12.13-apppkg` — see [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md).

**After `python312-vs2022-full-lab-2026-08-26` (local WIP, not in that tag):** realign stock **`Python-3.12.13/`** with upstream CPython + consolidate UEFI deltas under **`PyMod-3.12.13/`** only — build/deploy/verify before committing that follow-up.

---

## Single codebase — one branch for GCC and VS2022 (2026-08-28)

**Goal:** One git branch for manufacturing and development — **no separate GCC-only vs VS2022-only code lines.**

| Question | Answer |
|----------|--------|
| **Can `feature/python-3.12.13-vs2022` be that branch?** | **Yes.** Same **`Python312.inf`** / **`Python312_MIN.inf`**, same **`PyMod-3.12.13/`**, same Phase 8 vendored trees. EDK selects **`| GCC`** vs **`| MSFT`** sources and flags per **`build -t`**. |
| **Where is the GCC AppPkg port?** | **`feature/python-3.12.13-apppkg`** — **reference only** (historical GCC milestones, layout parity with **3.6.8 AppPkg** structure). **Do not use for day-to-day builds.** Manufacturing GCC uses **`feature/python-3.12.13-vs2022`**. As of **2026-08-28**, apppkg has **no commits vs2022 lacks** (merge-base **`a6c0cbd7`**; vs2022 **+23**). |
| **Two ports?** | **No** — one port with toolchain splits. Details: [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) §1. |
| **Build commands (same branch, same clone)** | **GCC:** WSL — [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md), **`create_python_pkg.sh GCC …`**. **VS2022:** Windows — [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md), **`create_python_pkg.bat VS2022 …`**. |
| **Shared PyMod on vs2022 affects GCC too** | Commits after apppkg merge-base (e.g. **`Lib/ssl/`**, **`rand_rdrand.nasm`** elf64+win64, **`rand_efi.c`**, REPL/readline stub policy) apply when you **`build -t GCC`** from this branch — **run GCC regression on vs2022 tip**, not stale apppkg tip. |
| **Runtime differences** | Same sources; **GCC** uses **`edk2_switch_stack` + `py_install_idt`**; **VS2022** uses **`PY_UEFI_MSVC_368_ENTRY`** — deviations **§11**. Not a reason for two branches. |
| **End state (locked 2026-08-28)** | **`feature/python-3.12.13-vs2022`** is the **only** active branch for GCC + VS2022 builds and upstream PR work. **No merge** vs2022 → apppkg. **`feature/python-3.12.13-apppkg`** stays frozen as **GCC / 3.6.8-structure reference** (tags **`python312-apppkg-8.x`**, [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md)). **Pre-upstream-push cleanup** on **vs2022** is still required before final edk2-libc contribution. |

---

| Area | State |
|------|--------|
| **VS2022 MIN** (`Python312_MIN.inf`, default DSC) | **Build + manufacturing UEFI runtime Done** — **`-h`**, **`-S -c`**, default / **`-S`** REPL, **`exit(0)`** → Shell → **`exit`** → firmware; **`import readline`** without env stays stub-safe |
| **MSVC entry** | **`PY_UEFI_MSVC_368_ENTRY=1`** — **`ShellCEntryLib`** on Shell stack (no custom stack switch / IDT); **GCC still uses** **`edk2_switch_stack` + `py_install_idt`** (see deviations §11) |
| **REPL / readline vs GCC** | **Default:** stdio REPL on **`os.name == 'uefi'`** (both toolchains). **Optional GCC pyreadline** (env **`PY_UEFI_READLINE=1`** + **`import readline`**) — **pass** 2026-09-01, teardown OK — **§ UEFI REPL / pyreadline**. **VS2022** manufacturing: stdio only; pyreadline opt-in **not** re-lab’d |
| **Frozen / deepfreeze** | **`PyMod-3.12.13/Python/frozen_modules/*.h`**, **`frozen.c`**, and **`deepfreeze/deepfreeze.c`** are **committed** (latin1 + **`statically_allocated`**). Fresh clone: no regen. Regen only when changing frozen inputs — **`Tools/build/regen_frozen_windows.cmd`** → PyMod outputs — see runtime notes §5 |
| **VS2022 FULL** (`BUILD_PYTHON312_FULL=TRUE`, `Python312.inf`) | **Build + lab Done** — Phase 8 **`-S -c`**, stdio **`-S`** REPL, Shell **`exit`** (2026-08 one-liners; 2026-09-01 **`-S`**) |
| **GCC FULL** (same branch, **`build -t GCC`**) | **Build + lab Done** (2026-09-01, **`dbc8416c`**) — Phase 8 matrix, stdio **`-S`**, optional pyreadline; edk2-py312 **`PACKAGES_PATH`** |
| **Unified branch (GCC + VS2022)** | **`feature/python-3.12.13-vs2022`** — **hardware sign-off both toolchains** — see **§ Single codebase** |
| **GCC regression** | **2026-09-01** FULL on vs2022 tip — **Done** (see lab note). Re-run after shared PyMod/INF edits. |
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
| V6 | Runtime smoke (MIN → FULL) | **Done (lab)** — MIN **Done**; FULL Phase 8 **`-S -c`** + stdio **`-S`** REPL on **GCC** and **VS2022**; optional GCC pyreadline (2026-09-01) |
| V7 | Docs and CI (`build-python312-uefi-vs2022.yaml`) | **Partial** (build guide, deviations doc, this status; no 3.12 CI) |
| V8 | Vendored FULL on VS2022 (8.1→8.2→8.5→8.3→8.4) | **Done** (same monolithic INF as GCC; MSFT-specific glue only) |

**Legend:** Not started · In progress · Partial · Blocked · Done · Skipped

### Mapping to GCC AppPkg phases

| GCC AppPkg | VS2022 |
|------------|--------|
| 0–1 Scaffold | **V1** workspace |
| 2 PyMod / pyconfig | **V2** + PyMod MSVC fixes in **V4** |
| 3 Frozen | **Done** — artifacts under **`PyMod-3.12.13/Python/`** (committed; §6 in WSL/Windows guides) |
| 4 INF MIN | **V3–V4** MSFT options + splits |
| 5 DSC / patches | **V1** + same patch policy as GCC |
| 6 MIN smoke | **V4–V6** |
| 7 Docs / CI | **V7** |
| 8 Vendored FULL | **V8** (each batch + `build -t VS2022`) |

---

## Locked decisions (VS2022 track)

| Item | Choice |
|------|--------|
| Baseline | GCC **Phase 8 FULL** behavior — originally developed on **`feature/python-3.12.13-apppkg`** (reference); **verify on `feature/python-3.12.13-vs2022` tip** |
| **Manufacturing branch** | **`feature/python-3.12.13-vs2022`** only — **`build -t GCC`** (WSL) and **`-t VS2022`** (Windows); **§ Single codebase** |
| **`feature/python-3.12.13-apppkg`** | **Reference archive** — GCC port phases, 3.6.8 AppPkg structural alignment; **not** merged into from vs2022 |
| `PACKAGES_PATH` | `<edk2>;<edk2-libc>` only — no sandbox LibFFI/OpenSSL/zlib packages |
| INF | **`Python312_MIN.inf`** + **`Python312.inf`** via **`BUILD_PYTHON312_FULL`** in DSC (no `!if` inside INF); MSFT **`PY_UEFI_MSVC_368_ENTRY`** for runtime — see runtime notes |
| `pyconfig` source of truth | **`PyMod-3.12.13/Include/pyconfig.h`** + **`efi/Include/pyconfig.h`** → **`srcprep.py`** |
| **`Python-3.12.13/` tree** | Must match **upstream CPython 3.12.13** for forked paths; **do not commit** `Lib/ssl/` or other srcprep overlays under stock tree — only **`PyMod-3.12.13/`** (see **`Tools/restore_upstream_from_cpython.py`**) |
| MSVC sizing | **`/DUEFI_MSVC_64`** on X64; **`_MSC_VER`** LLP64 (`SIZEOF_LONG` 4); GCC **`#else`** LP64 (8) |
| StdLib patches | **Target policy:** apply **`patches/*.patch`** locally; **do not commit** `StdLib/` / **`StdLibPrivateInternalFiles/`** — see **§ Locked policy — StdLib patches** and **§ Pre-upstream-push cleanup** (this branch currently **drifts** from that policy) |
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

### 2026-09-01 — GCC FULL on vs2022 branch (lab + docs)

1. **Hardware:** Phase 8 **`-S -c`** matrix + Shell **`exit`** on **`feature/python-3.12.13-vs2022`** @ **`dbc8416c`** — **pass** (see **§ GCC FULL regression — build and smoke**).
2. **Build:** GCC **`py312_boot_print_ascii`** guard in **`edk2main.c`**; frozen artifacts under **`PyMod-3.12.13/Python/frozen_modules/`** (in git — WSL guide §6).
3. **Optional pyreadline (GCC only):** **`PY_UEFI_READLINE=1`**, **`import readline`**, history/Tab, teardown — **pass**; **§ UEFI REPL / pyreadline** + lab note.
4. **Docs:** **`0b9fe05f`** (pyreadline); migration status lab build/smoke tables.
5. **V6 close:** FULL stdio **`Python312.efi -S`** (no pyreadline) — **GCC** + **VS2022** hardware **pass** (trivial **`>>>`**, **`exit(0)`**, Shell **`exit`**).

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
| V1.5 | Frozen + **`deepfreeze.c`** | **Done** — **`PyMod-3.12.13/Python/frozen_modules/`** (24× `.h`) + **`frozen.c`** + **`deepfreeze.c`** in git |
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
| V6.2 | FULL: `zlib`, `readline`, `ctypes`, `hashlib`, `ssl` | **Done (lab)** — VS2022 + GCC **`-S -c`** + stdio **`-S`** REPL + Shell **`exit`** |
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

#### FULL — signed off (Phase 8 **`-S -c`** 2026-08 / 2026-09-01; stdio **`-S`** REPL 2026-09-01)

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

**Not in manufacturing matrix (optional development):** **`PY_UEFI_READLINE=1`** pyreadline — see **§ UEFI REPL / pyreadline** and runtime notes §10.

#### Optional — pyreadline on GCC (not manufacturing default)

```text
set PY_UEFI_READLINE 1
Python312.efi -S
```

At **`>>>`**: **`import readline`** **before** arrow keys / Tab. Then verify history, Tab, **`exit()`** → Shell **`exit`** → BIOS. **GCC lab 2026-09-01: pass.** Do **not** assume **VS2022** without re-test.

---

### Phase V6 result

**MIN (VS2022):** Session 10 (2026-07-23) — **`-h`**, REPL, stub **`import readline`**, Shell **`exit`** signed off.

**FULL (VS2022, lab 2026-08-26 / 2026-08-27):** Phase 8 **`-S -c`** + Shell **`exit`** — no hang. See **§ Git tags** / lab notes.

**FULL (GCC, lab 2026-09-01):** Phase 8 **`-S -c`** @ **`dbc8416c`** — **pass**. Stdio **`-S`** REPL — **pass**. Optional **pyreadline** — **pass** (GCC only). Details: [`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md), migration **§ UEFI REPL / pyreadline**.

**FULL (VS2022):** Phase 8 **`-S -c`** (2026-08 lab) + stdio **`-S`** REPL (2026-09-01) — **pass**.

**V6 runtime (MIN + FULL):** **Done** on **`feature/python-3.12.13-vs2022`** for manufacturing stdio REPL and Phase 8 one-liners.

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

Run from **`feature/python-3.12.13-vs2022`** (same branch as VS2022 — **§ Single codebase**). After **`Python312.inf`**, **`pyconfig.h`**, or **any shared PyMod** edit (ssl, openssl, REPL, deepfreeze):

1. WSL: **`git apply`** patches if needed (on this branch often **skip** — **§ Branch drift**) → **`srcprep.py`** → **`BUILD_PYTHON312 -t GCC`** + **`BUILD_PYTHON312_FULL=TRUE`** → **`create_python_pkg.sh`**
2. **`vs2022_verify/verify_pyconfig_gcc.sh`**
3. On Windows (after pyconfig edits): **`vs2022_verify/verify_pyconfig_msft.bat`**
4. UEFI smokes: REPL/Shell **`exit`**; **`import ssl; ssl.create_default_context(); print('ok')`** (shared **`rand_rdrand.nasm`** / **`rand_efi.c`**)

Last known green GCC FULL: **2026-09-01** on **`feature/python-3.12.13-vs2022`** @ **`dbc8416c`** (lab note above). Re-run after shared PyMod/INF edits.

---

## Known issues / follow-ups

1. ~~**V6 MIN runtime smoke**~~ — **Done** (VS2022, 2026-07-23). ~~**FULL Phase 8 `-S -c` (VS2022 + GCC)**~~ — **Done**. ~~**GCC optional pyreadline**~~ — **Done** 2026-09-01. ~~**FULL stdio `-S` REPL (GCC + VS2022)**~~ — **Done** 2026-09-01.
2. ~~**WSL GCC regression on vs2022 tip**~~ — **Done** 2026-09-01 (**`dbc8416c`**).
3. **VS2022 vs GCC runtime** is **not** identical for firmware entry and interactive REPL — documented in **GCC deviations §11**; do not assume GCC pyreadline behavior applies to VS2022 manufacturing.
4. Re-**`git apply`** **`patches/*.patch`** after **`StdLib/`** cleanup **only if** those trees were reset to unpatched upstream (see **§ Branch drift — StdLib already patched**). On **`feature/python-3.12.13-vs2022`** today, **`git apply`** often fails with *already exists* / *patch does not apply* — that usually means patches are **already** in the tree; skip apply and build.
5. **`_ctypes_test`**: compiled **`| GCC`** only; excluded from UEFI **`config.c`** on both toolchains.
6. Do not put port tools under CPython **`Tools/`** — use **`vs2022_verify/`**.
7. **`build-python312-uefi-vs2022.yaml`** (**V7.5**) not added.
8. **Frozen / deepfreeze:** **`PyMod-3.12.13/Python/frozen_modules/*.h`**, **`frozen.c`**, **`deepfreeze.c`** committed; regenerate with **`Tools/build/regen_frozen_windows.cmd`** only when frozen **`.py`** inputs change.

---

## Next actions (recommended)

**Branch:** **`feature/python-3.12.13-vs2022`** for **both** toolchains — sole manufacturing line (**§ Single codebase**). **`feature/python-3.12.13-apppkg`** = reference only.

**Follow:** [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) · [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md) · [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10 · **§ V6** smoke commands

**Order:**

1. ~~**FULL stdio REPL**~~ — **Done** 2026-09-01 (**GCC** + **VS2022** FULL, no **`PY_UEFI_READLINE`**).
2. **Upstream / PR** — from **`feature/python-3.12.13-vs2022`** after **§ Pre-upstream-push cleanup**.
3. **Cleanup (optional):** **`PY_UEFI_BOOT_TRACE`** (or document GCC/MSFT split), StdLib **`Main.c`** probes, **`Py_DEBUG`** in UEFI **`pyconfig.h`**.
4. **V7:** **`build-python312-uefi-vs2022.yaml`** (matrix **GCC + VS2022**), **`Py312ReadMe.txt`** VS2022 section.
5. **Before final upstream edk2-libc PR:** **§ Pre-upstream-push cleanup**.
6. **Later:** host **GCC toolchain upgrade** + one rebuild/smoke (separate from branch validation).
7. **Future (not manufacturing):** VS2022 **pyreadline** opt-in re-test (GCC opt-in **pass** 2026-09-01).

---

## Locked policy — StdLib patches

Same as GCC AppPkg status (**target** for **`jpshivakavi/edk2-libc-jp`** / tianocore edk2-libc contribution):

- **Do not commit** applied **`StdLib/`** / **`StdLibPrivateInternalFiles/`** on the port branch.
- **Required** for a **clean** upstream libc checkout before build:  
  `git apply --ignore-whitespace AppPkg/Applications/Python/Python-3.12.13/patches/0001-*.patch` … **0004** (one-by-one; see build guide §4).
- MSVC-safe fixes for patched StdLib: commit **`patches/0001`**, **`patches/0002`** only (patch files under **`AppPkg/Applications/Python/Python-3.12.13/patches/`**).

### Branch drift — StdLib already patched (2026-08-27)

**Documentation** (build guide §4, table above) says patches are **local-only**. **`feature/python-3.12.13-vs2022`** on **`edk2-libc-jp-vsfix`** **already tracks** the patched StdLib tree in git (e.g. **`StdLib/LibC/Uefi/upipe.c`**, **`fdstat.c`**, console/ANSI/ioctl changes from **0001–0004**).

| Effect | Detail |
|--------|--------|
| **Building** | **Fine** — no need to **`git apply`** on a fresh **`git clone`** of this branch; patched files are already present. |
| **`git apply` on same branch** | **Fails** with *already exists* / *patch does not apply* — **expected**; not a broken tree. |
| **Upstream push** | **Not aligned** with locked policy — patched **`StdLib/`** must be **removed from git history on the branch** before the **final** push / PR to the shared edk2-libc repo (keep **`patches/*.patch`** only). |

**Verify patches are present (skip apply):**

```cmd
dir StdLib\LibC\Uefi\upipe.c
dir StdLib\LibC\Uefi\fdstat.c
findstr upipe StdLib\LibC\Uefi\Uefi.inf
```

---

## Pre-upstream-push cleanup (before final push to edk2-libc)

Complete before opening the **final** PR / merge to **`jpshivakavi/edk2-libc-jp`** (or upstream tianocore edk2-libc). Manufacturing can keep using the current branch until this is done.

| # | Item | Action |
|---|------|--------|
| 1 | **StdLib patch dirt in git** | Reset **`StdLib/`** and **`StdLibPrivateInternalFiles/`** to **upstream edk2-libc baseline** on the branch; ensure **only** **`AppPkg/Applications/Python/Python-3.12.13/patches/0001–0004`** remain the source of libc deltas. Re-verify: clean clone + **`git apply`** (all four) succeeds. |
| 2 | **Build guide / status wording** | Restore “apply patches every checkout” as the **consumer** workflow once (1) is done; remove or shorten **§ Branch drift** when drift is fixed. |
| 3 | **Stock `Python-3.12.13/`** | Upstream CPython 3.12.13 in git; UEFI **`.py`/`.h`** overlays only via **`PyMod-3.12.13/`** + **`srcprep.py`** — no committed **`Lib/ssl/`** under stock tree (see table **PyMod source of truth**). |
| 4 | **Debug scaffolding** | Remove or gate **`PY_UEFI_BOOT_TRACE`**, StdLib **`Main.c`** boot probes, optional **`Py_DEBUG`** in UEFI **`pyconfig.h`** when FULL is stable (see **Current status** table). |
| 5 | **One-off / obsolete tools** | ~~**`uefi_ssl_wrap*.py`**~~ removed from workspace; PyMod README updated (**`0e34cb60`**). Keep **`Lib/ssl/`** as only ssl path. |
| 6 | **Lab / tag** | Keep tag **`python312-vs2022-full-lab-2026-08-26`** → **`3568d02d`** as manufacturing reference; re-tag only after (1)–(3) if rebuild is required. |

**After (1):** document in **`Python312_Windows_VS2022_Build_Guide.md`** §4 that **`git apply`** is **required** again on every clean libc checkout.

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
