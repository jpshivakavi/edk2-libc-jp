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
**Updated:** 2026-09-09 (MIN VS2022 signed off — §7.1; two `edk2_seh_*` defects fixed)  
**SEH fault recovery design:** [`Python312_SEH_Fault_Recovery_Design.md`](./Python312_SEH_Fault_Recovery_Design.md) (§2 fixes landed; API not implemented)  
**Strategy:** **Single line:** **`feature/python-3.12.13-vs2022`** for **`build -t GCC`** and **`-t VS2022`**. **`feature/python-3.12.13-apppkg`** kept as **read-only reference** (GCC port / 3.6.8 AppPkg structure alignment) — **no merge back into apppkg**. Same `PACKAGES_PATH=<edk2>;<edk2-libc>`; vendored libs in **`PyMod-3.12.13/Modules/`**  
**Branch:** **`feature/python-3.12.13-vs2022`** — sole manufacturing line (forked from **`feature/python-3.12.13-apppkg`**; apppkg now **reference only**)  
**Target repo:** `jpshivakavi/edk2-libc-jp` (push from **`edk2-libc-jp-vsfix`** when ready)  
**Windows WORKSPACE:** `c:\Users\njayapra\github\edk2` (tianocore/edk2 — `edksetup.bat`, `Build\`)  
**Libc clone / `EDK2_LIBC_PATH`:** `c:\Users\njayapra\github\edk2-libc-jp-vsfix` (active VS2022 workspace; branch **`feature/python-3.12.13-vs2022`**)  
**WSL GCC regression:** **2026-09-04** — FULL @ **`3afa03f5`** (WSL clone at **`b60db389`**, edk2-py312 layout); Phase 8 spot-check + Shell **`exit`** + relaunch, **same code state as VS2022** — [`Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md`](./Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md). Prior: **2026-09-01** @ **`dbc8416c`** — full Phase 8 matrix + pyreadline ([`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md)); **2026-07-20** MIN/prep only.  
**MSVC reference INF:** [`Python-3.6.8/Python368.inf`](./Python-3.6.8/Python368.inf)  
**3.6.8 VS2022 CI:** [`.github/workflows/build-python-uefi-vs2022.yaml`](../../../.github/workflows/build-python-uefi-vs2022.yaml) (`BUILD_PYTHON368` only today)

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
**Last code-bearing commit:** **`3afa03f5`** — srcprep/deepfreeze helper cleanup (commits after it are docs-only). Prior GCC build fix: **`dbc8416c`** (**`py312_boot_print_ascii`** in **`edk2main.c`**).

**Manufacturing sign-off (same branch):**

| Toolchain | Lab | Commit (reference) | Phase 8 **`-S -c`** + Shell **`exit`** | FULL stdio **`Python312.efi -S`** → **`exit(0)`** → Shell **`exit`** |
|-----------|-----|-------------------|----------------------------------------|-----------------------------------------------------------------------------|
| **VS2022 FULL** | 2026-08-26 / 27; REPL 2026-09-01; post-PyMod 2026-09-04 | **`4dec4edf`** (ssl/RNG), **`3568d02d`** (import ssl); **`3afa03f5`** | **Done** — [`Python312_VS2022_Lab/`](./Python312_VS2022_Lab/); re-confirmed post-PyMod | **Done** (2026-09-01) — no **`PY_UEFI_READLINE`** |
| **GCC FULL** | 2026-09-01; post-PyMod 2026-09-04 | **`dbc8416c`**; **`3afa03f5`** | **Done** — Phase 8 matrix + optional **pyreadline** (**§ UEFI REPL / pyreadline**); re-confirmed post-PyMod | **Done** (2026-09-01) — no **`PY_UEFI_READLINE`** |

**Post-PyMod regression — VS2022 **and** GCC FULL, 2026-09-04 @ `3afa03f5`:** Phase 8 spot-check (**`sys.version`**; **`zlib, ctypes, hashlib`**; **`ssl.create_default_context()`**) + Shell **`exit`** + relaunch — **pass on both**. Both images built from the **same code state** (Windows clone and WSL clone both at **`b60db389`**; later commits docs-only), image identity confirmed by hash against build output. First hardware run on **either** toolchain **after** PyMod consolidation (**`9d465ec2`**), frozen-header relocation (**`55219522`**) and the deepfreeze helper path fix (**`0a674ac0`**) — relocated frozen/deepfreeze artifacts cleared on **both** MSVC 368 and GCC stack-switch entry paths. Gap-closing follow-ups run the same day — **`ctypes.sizeof(c_void_p)`** → **`8`** (LLP64 / **`UEFI_MSVC_64`** canary) and **`import zlib, ssl, ctypes, hashlib`** → **`phase8 ok`** (single process) — **pass on both**, teardown clean. **Every check guarding a known historical failure on this port is now green at this code state** (OpenSSL RNG, `socket.py`/`selectors`, LLP64 pointer width, deepfreeze static strings, finalize/re-entry). Still open: §2 baseline rows, itemised REPL rows, all pyreadline checks — [`Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md`](./Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md).

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
| **`PY_UEFI_READLINE=1`** + **`import readline`** + history/Tab + teardown | **GCC only** (2026-09-01 @ **`dbc8416c`**; re-run 2026-09-07 @ **`3afa03f5`**) | **Pass** |
| pyreadline **phases 1–5** incl. stub-vs-real assertions, non-interactive opt-in, non-bugs, cleanup | **GCC** (2026-09-07 @ **`3afa03f5`**) | **Pass** — [`2026-09-07_GCC_FULL_pyreadline_phases.md`](./Python312_VS2022_Lab/2026-09-07_GCC_FULL_pyreadline_phases.md) |
| Same pyreadline opt-in on **VS2022** FULL | **Run 2026-09-07** @ **`3afa03f5`** | **HANG reproduces** — phases 1/5 (stub) clean, phases 2/3 hang Shell **`exit`**. **Import alone is sufficient; no interactive REPL needed.** **Boot trace same day: Python teardown is NOT the cause** — ladder completes through **`before return from UefiMain`** and the prompt returns; timer **never created**. [`2026-09-07_VS2022_FULL_pyreadline_hang.md`](./Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md) · deviations **§11.3**, runtime notes **§10** |

**Takeaway:** **GCC** can use **optional pyreadline** with current teardown sources when env + **`import readline`** are used — now verified through the full **phase 1–5** procedure at the **pinned post-PyMod state** (**`3afa03f5`**, 2026-09-07), including assertions that the manufacturing default really loads the **stub** (`_ReadlineStub`, with neither `pyreadline` nor `edk2console` in `sys.modules`). **VS2022 manufacturing** stays **stdio default** (FULL **`-S`** signed off 2026-09-01); **VS2022 pyreadline** opt-in remains **not** re-lab’d, and §5.2/§5.3 have **never** been run there.

**Env volatility (applies to any of these tests):** plain **`set NAME value`** is **non-volatile** and **survives reboot** — `ShellPkg` **`Set.c`** passes **`-v`** straight through as the `Volatile` argument of `ShellSetEnvironmentVariable` (*"non-volatile (FALSE) or volatile (TRUE)"*). Use **`set -v PY_UEFI_READLINE 1`** so a forced power-cycle clears it; **`set -d PY_UEFI_READLINE`** deletes either kind. A lingering variable silently invalidates later stdio-default runs.

**Cross-refs:** [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10 · [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) §11.3 · lab [`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md)

---

## Git tags (VS2022 track)

| Tag | Commit | Meaning |
|-----|--------|---------|
| **`python312-unified-full-lab-2026-09-04`** | **`779b20cc`** | **Current clone-and-build pin — unified FULL, post-PyMod (2026-09-04):** **`feature/python-3.12.13-vs2022`** — **GCC** + **VS2022** FULL built from the **same code state** (**`3afa03f5`** last code-bearing commit). Phase 8 **`-S -c`**, **`ssl.create_default_context()`**, **`ctypes.sizeof(c_void_p)`**→**`8`**, **`import zlib, ssl, ctypes, hashlib`**→**`phase8 ok`**, Shell **`exit`** → firmware, relaunch — **pass on both**. **First pin that includes the PyMod-3.12.13 consolidation** (**`9d465ec2`**), frozen headers under PyMod (**`55219522`**) and the deepfreeze helper path fix (**`0a674ac0`**) — so a fresh clone of this tag builds without regen. All **known-historical-failure guards green**. Lab: [`2026-09-04_unified_FULL_post_pymod_smoke.md`](./Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md). The tag **message** says pyreadline is not covered; that was true when cut, and **superseded for GCC on 2026-09-07** — phases 1–5 pass at this same code state ([`2026-09-07_GCC_FULL_pyreadline_phases.md`](./Python312_VS2022_Lab/2026-09-07_GCC_FULL_pyreadline_phases.md)). Tag text is immutable, so treat that lab note as the correction of record. **VS2022 pyreadline is still not covered.** |
| **`python312-unified-full-lab-2026-09-01`** | **`753bfefb`** | **Superseded** by the 09-04 pin for clone-and-build — **predates PyMod consolidation**. **Unified FULL UEFI lab (2026-09-01):** **`feature/python-3.12.13-vs2022`** — **GCC** + **VS2022** FULL; Phase 8 **`-S -c`**, stdio **`Python312.efi -S`** → **`exit(0)`** → Shell **`exit`**; GCC optional **pyreadline**. Lab: [`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./Python312_VS2022_Lab/2026-09-01_GCC_FULL_vs2022_branch_regression.md). **V6 Done.** |
| **`python312-vs2022-full-lab-2026-08-26`** | **`3568d02d`** | **VS2022 FULL UEFI lab sign-off (2026-08-26):** `import sys` / **`import ssl`** / **`ssl.create_default_context()`** / **hashlib** / **ctypes** one-liners; **`Shell>`** → **`exit`** → BIOS/setup. PyMod **`Lib/ssl/`** (`_uefi_min`), MSVC teardown parity, post-finalize OpenSSL/console handoff. Details: [`Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md`](./Python312_VS2022_Lab/2026-08-26_VS2022_FULL_ssl_Shell_exit.md). |

**Checkout current unified manufacturing pin (post-PyMod):** `git fetch origin tag python312-unified-full-lab-2026-09-04 && git checkout python312-unified-full-lab-2026-09-04`

**Older unified pin (pre-PyMod, superseded):** `git fetch origin tag python312-unified-full-lab-2026-09-01 && git checkout python312-unified-full-lab-2026-09-01`

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

### 2026-09-07 — GCC pyreadline phases 1–5 on the pinned state

1. **Hardware (GCC FULL, `3afa03f5`):** smoke doc **§5.0 phases 1–5** — stub default asserted, non-interactive opt-in, interactive history/Tab, documented non-bugs, env cleanup — **all pass**, outputs matched expectations at every phase. Code state unchanged since 09-04 (all 11 intervening commits docs-only), so this is the state pinned by **`python312-unified-full-lab-2026-09-04`**.
2. **First run of the stub-vs-real assertions on any toolchain.** Earlier sign-offs showed a stub `import readline` did not break teardown but never asserted **which** path loaded; `_ReadlineStub` plus absent `pyreadline`/`edk2console` now confirms the manufacturing default never touches the console.
3. **GCC pyreadline moves off pre-PyMod code:** previously pinned to 2026-09-01 @ **`dbc8416c`**, which predates **`9d465ec2`**; now confirmed at **`3afa03f5`**, so the relocated `readline.py` / `pyreadline/` staging is verified rather than assumed.
4. **Traps confirmed on hardware** (both were source-derived): env value parsing is exact-match (**`True`** silently yields stub), and arrow keys before **`import readline`** give **`U+001B`**.
5. **Docs:** **`e8977117`** corrected the shell env volatility guidance — plain **`set`** is **non-volatile**; prefer **`set -v`** for tests.
6. **Lab note:** [`Python312_VS2022_Lab/2026-09-07_GCC_FULL_pyreadline_phases.md`](./Python312_VS2022_Lab/2026-09-07_GCC_FULL_pyreadline_phases.md).
7. **VS2022 pyreadline (same day, same code state) — HANG reproduces.** Phases 1 and 5 (stub) clean on the same image; phases 2 and 3 both leave Shell **`exit`** hanging. **Phase 2 hung with no interactive REPL and no keystrokes**, so **`import readline`** alone is the trigger — earlier docs describing this as a REPL/line-editing issue understated it. Interpreter returns to **`Shell>`** normally; only the firmware handoff hangs. Not persisted across power-cycle. [`Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md`](./Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md).
8. **Boot trace captured the same day — the fault is outside the Python image.** With **`PY_UEFI_BOOT_TRACE=1`** (already on MSFT **`CC_FLAGS`**), the phase 2 teardown ladder runs to completion: both **`edk2_console_detach_readline`** calls print **`enter`**/**`leave`**, then **`Py_FinalizeEx`** → **`after Py_BytesMain`** → **`after main()`** → **`after edk2_free_environ`** → **`before return from UefiMain`**, and the **`FS1:\EFI\bin\>`** prompt returns. Only the Shell's own **`exit`** hangs afterwards. **`stop_timer: already off`** on both detach calls proves the 1 ms periodic timer was **never created** — **`py_console_install_readline_hook`** deliberately skips it with a comment naming this exact hang, so that mitigation is in place and this is a **different** cause. **`edk2_console_handoff_to_shell`** never ran (its only non-Python call site is gated on **`py312_openssl_loaded`**, and this run did not import **`ssl`**). **Stop investigating Python-side teardown.** Two code defects surfaced while analysing: **`edk2_console_restore_for_shell()`** has **zero call sites**, and **no ConOut restore exists anywhere in the tree** despite two API contracts promising one (both paths only drain ConIn) — the latter is the leading suspect for phase 3. Lab note carries a three-run bisect (**`edk2console`** alone / full **`pyreadline`** tree without the hook / hook without **`pyreadline`**) to isolate the surviving side effect.
9. **Bisect ran the same day — `edk2console` is exonerated and this is probably not a readline defect.** **`import edk2console`** alone → **`ok`** and Shell **`exit`** reaches BIOS setup **clean**. **`import pyreadline.rlmain`** → **`ok`** then Shell **`exit`** **hangs**. That import provably constructs **no `Console`** and installs **no hook**: **`rl = Readline()`** and **`console.install_readline(rl.readline)`** are both in **`readline.py`** (lines 55/99), not in **`pyreadline/__init__.py`** or **`rlmain.py`**. So **`PyOS_ReadlineFunctionPointer`** was never set, **`get_output_mode_ex()`** was never called, **`read_history_file()`** never ran, and **`ensure_input()`** never ran — **no `edk2console` C entry point executes** beyond the bare **`PyInit_edk2console`** that run 1 proves harmless. What remains is only **module-level pure-Python import work**: **`logger.py`** → stdlib **`logging`** (dragging **`threading`**, **`weakref`**, **`atexit`** registration), plus **`traceback`**/**`re`**/**`keysyms`**/**`ansi`** and ~38 **`.py`** reads from FAT. (**`socket`** is imported only inside **`SocketStream.__init__`**, which never runs, so the known **`socket.py`**/**`selectors`** teardown issue is not in play.) **Reframe: likely "import enough pure Python — or specifically `logging` — and Shell `exit` hangs", which would affect any script.** Next: stdlib control runs (**`import logging`**; **`import re, traceback`**; both) and then a **rebuild-free** in-place bisect of **`EFI\lib\python3.12\pyreadline\__init__.py`** on the volume, since dotted imports cannot bisect a package **`__init__`**.
10. **Stdlib control ran the same day — THIS IS NOT A READLINE DEFECT.** **`Python312.efi -S -c "import logging; print('ok')"`** prints **`ok`** and then **Shell `exit` HANGS** — with **no readline, no `edk2console`, and no console I/O** anywhere in that command. It also explains the earlier runs: **`pyreadline/logger.py`** imports **`logging`**, so **both** hanging cases share exactly that one heavyweight import, while every clean case (**`import edk2console`**, Phase 8 C extensions, the phase 1 stub) avoids it. **Threading is ruled out by inspection:** **`pyconfig.h`** sets **`_POSIX_THREADS`**/**`HAVE_PTHREAD_H`** so **`thread_pthread.h`** is used, and the pthread symbols come from **`PyMod-3.12.13/efi/src/dummy_pthread.c`** — pure static-array bookkeeping (**`mutexes[256]`**, **`conds[256]`**, **`keys[256]`**) with **no `gBS` call, no `CreateEvent`, no firmware state** — so the **`logging/__init__.py:232`** **`threading.RLock()`** cannot affect the Shell; the **`:2281`** **`atexit.register(shutdown)`** is likewise cleared (trace shows **`after _PyAtExit_Call`**). **With teardown, `edk2console`, hooks, timers, events and locks all eliminated, the only remaining residue of a cleanly-exited image is heap/pool footprint** — leading hypothesis is that BDS needs to allocate for the setup UI after Shell **`exit`** and cannot. **Re-scope: "VS2022 FULL — pure-Python import hangs Shell `exit`", pyreadline was only the first symptom.** Next discriminators (no rebuild): **`import json`** (bulky pure Python, no **`logging`**/**`threading`** — the key test), **`import threading`**, **`import re, traceback, warnings, weakref, collections.abc, string`**, and **`x = bytearray(16*1024*1024)`** (allocation only, zero imports — hangs ⇒ pool pressure proven).
11. **`import json` hangs too — `logging` is cleared as well; `re` is now the perfectly correlated import.** **`Python312.efi -S -c "import json; print('ok')"`** → **`ok`** then Shell **`exit`** **hangs**, and **`json`** pulls neither **`logging`** nor **`threading`**. What the two hanging stdlib cases share is **`re`**: **`json/decoder.py`**, **`json/encoder.py`** and **`json/scanner.py`** each **`import re`** at line 3, and **`logging/__init__.py:26`** imports it too. **`re`** in turn drags **`enum`**, **`functools`**, **`collections`**, **`types`**, **`operator`**, **`reprlib`**, **`copyreg`**. **This also explains why Phase 8 always looked clean:** this port's staged **`Lib/ssl/__init__.py`** is the UEFI-minimal variant importing **only `os`**, so the FULL smoke matrix barely touches the pure-Python stdlib — **`re`**-class import weight was never covered by any signed-off test. **Correlation is not yet mechanism:** ~20 modules is also ~20 more file opens plus a few hundred KB of heap versus any clean run. **Two mechanisms remain open — pool footprint, or a per-import file-handle leak** (leaked **`EFI_FILE_PROTOCOL`** handles would keep the FAT volume's open count non-zero, which survives image exit and would stall volume teardown at Shell **`exit`**). Next three runs, each with **zero or minimal imports** so they separate mechanism from module: **`import re`**; **`x = bytearray(16*1024*1024)`** (memory only, no file opens); **`[open('Python312.efi','rb').close() for i in range(50)]`** (file opens only, negligible heap).
12. **All three mechanism runs CLEAN — `re`, raw footprint and file-handle leaks all eliminated.** **`import re`** → clean; **`x = bytearray(16*1024*1024)`** → clean; 50 × **`open`**/**`close`** → clean. Consequences: (a) **`re`** and its whole tree (**`enum`**, **`functools`**, **`collections`**, **`abc`**, **`reprlib`**, **`types`**, **`operator`**, **`copyreg`**, **`_sre`**) are **not sufficient**; (b) **raw heap footprint is not the metric** — a single **16 MB** block is far more memory than ~20 stdlib modules consume, yet exits clean; (c) the StdLib/FAT file layer does **not** leak per open; (d) **`_thread`** is now cleared *empirically* as well as by inspection, because **`import re`** pulls **`functools`**, whose line 21 is **`from _thread import RLock`** — so locks are allocated in a **clean** run. **The margin is razor thin:** **`import re`** already loads 15+ modules and passes, while **`import json`** adds only ~5 (**`json`**, **`json.decoder`**, **`json.encoder`**, **`json.scanner`**, **`_json`**) and hangs — strongly suggesting a **threshold**. Remaining candidates: **allocation count / pool fragmentation** (imports create tens of thousands of *small* long-lived objects, requesting many arenas from the EFI pool — an allocation *shape* the single 16 MB block never tested), a **count-based limit** on modules/arenas/a fixed table, or **C-stack depth** in the import machinery (note this port uses a stack-switch entry path, **`PY_UEFI_MSVC_368_ENTRY`**, so stack headroom is not the firmware default). Next: **measure** the boundary with **`print(len(sys.modules))`** for baseline / **`re`** / **`json`** / **`logging`** (the outer two are known-clean, inner two known-hanging, so the counts bracket it), then test allocation shape with **`x = [bytes(64) for i in range(200000)]`** and **`x = [{} for i in range(100000)]`** (no imports), plus a one-line stack probe **`x = eval('['*200 + ']'*200); print(len(repr(x)))`**.
13. **2026-09-08 — threshold measured at 43–48 modules; `.pyc` writes to FAT are the new lead.** **`len(sys.modules)`**: **23** at the **`-S`** baseline and **42** after **`import re`** — both **clean**; **48** after **`import json`** and **65** after **`import logging`** — both **HANG**. So the boundary is **19–25 newly imported modules**. **Not a descriptor limit:** **`StdLib/Include/sys/syslimits.h:56`** sets **`OPEN_MAX 255`** (**`FOPEN_MAX`** follows it). **Gap in the earlier testing:** the 50-cycle file test used **`open(..., 'rb')`** — read-only — so it never created, wrote, renamed or deleted anything, and the write path was never cleared. **This port writes on every import:** **`create_python_pkg.sh`** deliberately excludes **`__pycache__`** (lines 107/117/136) so the staged volume ships with **no precompiled bytecode**; **`importlib/_bootstrap_external.py:1141`** gates only on **`not sys.dont_write_bytecode`**; **`:1235`** creates the **`__pycache__`** directory and **`:1245`** calls **`_write_atomic`**, which (**`:201`**) writes a **`.tmp`** sibling then **`_os.replace(path_tmp, path)`**, falling back to **`_os.unlink`** on **`OSError`**. That is a directory create + temp create + write + **rename** on FAT through the firmware **per import** — ~19 in the clean **`re`** run vs ~25 in the hanging **`json`** run, straddling the measured boundary. Unflushed/half-completed FAT mutation is exactly the residue that survives image exit and could stall volume teardown at Shell **`exit`**; orphaned **`.tmp`** files would additionally indicate **`_os.replace`** is unsupported here. **Decisive one-boot test, no rebuild:** **`-B`** is supported (**`PyMod-3.12.13/Python/sysmodule.c:3026`** maps **`dont_write_bytecode`** → **`-B`**), so run **`Python312.efi -B -S -c "import json; print('ok')"`** — clean ⇒ bytecode writing is the mechanism. Zero-boot check: **`ls FS1:\EFI\lib\python3.12\json\__pycache__`** for **`.pyc`**/stray **`.tmp`**. **If confirmed, fix is either** pre-generating **`__pycache__`** at package time (drop the exclude + **`compileall`** — *caveat:* **`.pyc`** marshal magic must match this exact 3.12.13 build, a trap this port has hit before) **or** shipping with **`-B`**/**`PYTHONDONTWRITEBYTECODE`**.
14. **2026-09-08 — `-B` still hangs: bytecode writing is NOT the mechanism.** **`Python312.efi -B -S -c "import json; print('ok')"`** → **`ok`** then Shell **`exit`** **hangs**. **`json`** was unchanged and the only removed variable was the **`__pycache__`** write, so **`.pyc`** writing, **`__pycache__`** mkdir, **`_write_atomic`** and **`_os.replace`** are all **cleared** in one boot. (The packaging observation stands separately — the volume ships without bytecode so every launch pays full compilation — but that is a **performance** note, not this bug.) **C-stack depth is now downweighted by reasoning:** import nesting does not grow with module *count* (**`json`** → **`json.decoder`** → **`re`** → **`enum`** is about as deep as **`re`** → **`enum`**), whereas the observed 42→48 boundary is **cumulative**. **Leading hypothesis is now `obmalloc` arena count / EFI pool fragmentation** — the one mechanism that scales cumulatively with module count and that **no clean run tested**: the 16 MB **`bytearray`** is a **single** large **`malloc`** that **bypasses `obmalloc`** (large objects are not pooled), so it created **zero arenas** and left **one** hole, while ~48 modules create tens of thousands of small long-lived objects and make **`obmalloc`** request **many separate arenas** via **`AllocatePool`**. If the image exits without returning them the pool is left **fragmented rather than merely smaller**, and BDS then needs a large contiguous allocation for the setup UI after Shell **`exit`**. **Next: firmware-side evidence via `memmap`** — runnable **before** typing **`exit`** since the prompt returns normally, so compare free-page totals and descriptor counts at fresh boot / after a clean **`import re`** run / after a hanging **`import json`** run (Debug1 profile only). **Then measure the allocator:** **`sys.getallocatedblocks()`** at baseline / **`re`** / **`json`** (the small-object analogue of the **`len(sys.modules)`** bracket), and the **import-free allocation-shape** runs **`x = [bytes(64) for i in range(100000)]`** and **`x = [{} for i in range(100000)]`** — a hang there confirms allocation count/fragmentation with no module involved and points the fix at arena behaviour against **`AllocatePool`**; both clean would be a strong negative implying something specific to *loading modules* rather than raw allocation.
15. **2026-09-08 — ROOT CAUSE FOUND: VS2022 runs Python on the ~128 KB firmware stack; GCC gets 64 MB.** The decisive clue was the user's observation that **GCC never shows this hang**. In **`PyMod-3.12.13/efi/src/edk2main.c`** the toolchains take different entry paths: the **`#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_368_ENTRY)`** branch at **`:202`** calls **`ShellCEntryLib`** directly and **returns at `:212`** — *before* **`:215-216`**, where the other branch sets **`g_edk2_globals.stack_size = PY_UEFI_DEFAULT_STACK_SIZE`** and **`malloc`**s it, and *before* **`edk2_switch_stack()`** at **`:228`**. **`PY_UEFI_DEFAULT_STACK_SIZE`** is **`(64*1024*1024)`** = **64 MB** (**`efi/Include/efi/edk2stack.h:5`**), and **`PY_UEFI_MSVC_368_ENTRY=1`** is on the MSFT **`CC_FLAGS`** of **both** **`Python312.inf:1073`** and **`Python312_MIN.inf:327`**. **So every VS2022 image runs the entire interpreter on the UEFI firmware stack (~128 KB for a Shell app) while GCC runs on 64 MB — ~500× less headroom from the same commit.** **Compounding defect:** **`PyOS_CheckStack()`** (**`edk2main.c:249-256`**) compares **`rsp`** against **`g_edk2_globals.stack`**, which is assigned only at **`:216`** — after the early return — so it is **NULL** in VS2022 images, **`rsp > 0`** is always true, and the guard **always reports "stack is fine"**. It is not dead code: **`USE_STACKCHECK 1`** is set (**`PyMod-3.12.13/efi/Include/pyconfig.h:1816`**) and it is called from **`Objects/object.c`**, **`Python/ceval.c:260`**, **`Python/pythonrun.c:1913`** — **wired up and actively lying**, so deep recursion silently runs off the stack instead of raising **`RecursionError`**. **This explains every observation:** GCC clean (64 MB); the boot-trace ladder flawless through **`before return from UefiMain`** with the prompt returning (the overflow corrupts memory *below the firmware stack*, **outside** the image, so Python is undamaged and only BDS later trips on it); hang exclusively at Shell **`exit`**; **`import re`** (42 modules) clean but **`import json`** (48, and a **package**, so **`json/__init__`** → **`json.decoder`** → **`re`** → **`_compiler`** → **`_parser`** → **`_constants`** is ~2 frames deeper) hanging; the sharp threshold (a stack limit is a hard boundary); 16 MB **`bytearray`** clean (heap, shallow); 50 **`open`**/**`close`** clean (shallow); **`-B`** irrelevant. **Correction: module count was only ever a proxy — the real variable is peak C-stack depth**, and the earlier "depth is not cumulative so stack is unlikely" reasoning was wrong because nested *packages* do add frames. **Note `Python312_VS2022_GCC_Toolchain_Deviations.md` §11.1 already warned that "deep recursion, fault handling, and stack limits may differ between GCC and VS2022 images even from the same git commit"** — that prediction was correct and went unconnected. **Why the workaround exists:** **`edk2main.c:203-205`** and **`Python312.inf:1070-1072`** record that without the 368 path VS2022 **hung inside `ShellCEntryLib`** after the stack switch (boot stopped at **`before ShellCEntryLib`**) — so a **boot** hang was traded for this **exit** hang. **`edk2stack.nasm`**/**`edk2handler.nasm`** are **not** toolchain-tagged (**`Python312.inf:62,69`**), so **`edk2_switch_stack`** is already compiled into the VS2022 image — **the assembly is not the blocker**. **Fixes, cheapest first:** (1) **decouple the stack switch from the IDT** — **`edk2_switch_stack()`** (**`:228`**) and **`py_install_idt()`** (**`:231`**) are independent calls that the comments blame as a pair; build VS2022 with the switch but **skipping `py_install_idt()`** — if it boots, VS2022 gets the 64 MB stack and the bug is gone; (2) **fix `PyOS_CheckStack` for the 368 path** by capturing the firmware stack base at entry, converting silent firmware corruption into a clean **`RecursionError`** (worth doing regardless); (3) stopgap **`sys.setrecursionlimit()`** lower, though it counts Python frames not C frames. **Confirming test (no rebuild):** stage tiny modules and compare **deep-and-few** (**`t1`**→**`t2`**→…→**`t10`**, 10 modules at depth 10) against **shallow-and-many** (**`u1`**…**`u30`** each **`x = 1`**, 30 modules at depth 1) — deep hanging while shallow stays clean confirms depth and exonerates count.
16. **2026-09-08 — `PyOS_CheckStack` fix implemented and VALIDATED ON HARDWARE @ `c3819602`.** Fix 2 (safety, not capability) chosen deliberately: it leaves the 368 entry path untouched so there is **no boot-path risk**. Added **`uint64_t stack_limit`** to **`edk2_globals_t`**; added **`PY_UEFI_STACK_MARGIN`** (8 KB) and **`PY_UEFI_FIRMWARE_STACK_BUDGET`** (96 KB) to **`edk2stack.h`**, both **`#ifndef`**-guarded for **`CC_FLAGS`** override; **368 path** derives the bound from **`rsp`** at **`UefiMain`** entry, **GCC path** sets it from the **`malloc`**'d base plus margin and **clears it after `edk2_revert_stack()`** so a stale bound cannot be compared against freed memory; **`PyOS_CheckStack()`** returns **`0`** when the bound is unknown (preserving old permissive behaviour) and otherwise **`rsp <= stack_limit`**. Added a value-printing boot-trace line under the existing **`PY_UEFI_BOOT_TRACE=1`** (no flag change needed). **Hardware result — PASS:** trace shows **`firmware stack rsp=6A969618 limit=6A951618 budget=18000`** (arithmetic exact; non-zero **`limit`** proves the fix is live, it was effectively **`0`** before), and **`Python312.efi -S -c "import json; print('ok')"`** now raises **`MemoryError: stack overflow`** with a full traceback, completes teardown, and **Shell `exit` reaches BIOS setup with NO hang** — the behaviour this entire investigation was chasing. **Guard site:** the message is **lowercase**, so it fired from **`Objects/object.c`** (418/548/601, the **`PyObject_Repr`**/**`PyObject_Str`** **`USE_STACKCHECK`** guards) rather than the capitalised **`"Stack overflow"`** at **`Python/ceval.c:263`**; both sites exist and either can fire. **Depth data:** the traceback died **6 imports deep** (**`json`** → **`json.decoder`** → **`re`** → **`enum`** → **`functools`** → **`collections`**, inside **`cache_from_source`**) whereas **`import re`** reaches only **4** and stays clean — so the 96 KB budget trips between **4 and 6 nested imports**, the same band where the real firmware stack was already being breached, indicating the budget is **approximately calibrated to the hardware**. **Scope:** the **failure mode** is fixed (silent firmware corruption → catchable exception, reliable **`exit`**); the **capability** is not — **`json`**/**`logging`**/pyreadline remain unusable on VS2022 until the stack switch works under MSVC (cheapest experiment: switch the stack but **skip `py_install_idt()`**). **Expected new signature:** pyreadline phases 2/3 should now raise **`MemoryError`** instead of hanging, since **`pyreadline/logger.py`** pulls **`logging`**. **OPEN:** Phase 8 regression sweep not yet run on **`c3819602`** — expected to pass since Phase 8 imports are shallow (staged **`Lib/ssl/__init__.py`** imports only **`os`**; **`ctypes`**/**`hashlib`**/**`zlib`** are C extensions), but a too-tight budget would surface there first, so it must be verified rather than assumed. Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_FULL_stackcheck_fix.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_FULL_stackcheck_fix.md).
17. **2026-09-08 — the fix is only PARTIAL: the interactive REPL still hangs, so the 96 KB budget is too generous.** Same image, same commit: entering the **interactive** interpreter and running **`import json`** displays **`MemoryError`**, **`exit()`** returns to the Shell normally, but **Shell `exit` HANGS** — while the non-interactive **`-S -c "import json"`** stays clean. **Why the guard is insufficient by construction:** **`PyOS_CheckStack()`** is a **sampled** check, called only where CPython calls it (**`Objects/object.c`** **`PyObject_Repr`**/**`Str`**, **`_Py_CheckRecursiveCall`**); between two sample points arbitrarily much C stack can be consumed unchecked. The residual between **`stack_limit`** and the true stack base must therefore absorb the worst-case **unchecked excursion** *plus* the **`MemoryError`** unwind and traceback formatting. The REPL starts deeper (**`PyRun_InteractiveLoop`** → **`PyRun_InteractiveOne`** → parser → eval frames all live when the import begins) and formats the traceback from inside that deeper context, so with **96 KB** of a ~128 KB stack budgeted only ~32 KB of residual remains and the interactive path overruns it. The REPL only *appears* to recover for exactly the original reason — the corruption lands in firmware memory Python never touches again. **This is a strong argument for the capability fix (the stack switch):** a sampled guard against an **unknown** stack base admits no provably safe budget, so any number is a guess. **Instrumentation added rather than guessing a smaller number:** **`stack_entry_rsp`** and **`stack_min_rsp`** (deepest **`rsp`** ever sampled) added to **`edk2_globals_t`**, printed after **`ShellCEntryLib`** returns under the existing **`PY_UEFI_BOOT_TRACE=1`** as **`stack high-water min_rsp=… limit=… used=…`**. **`min_rsp`** under-reports the true peak but **brackets the stack base** — a value from a **clean** run lies **above** it, one from a **hanging** run at or **below** it — so capturing it from the clean **`-c`** run and the hanging interactive run pins the base between the two and lets the budget be set from measurement. **Do not simply lower the budget blind:** it risks breaking shallow imports (**`import re`**, Phase 8) with no evidence, which is exactly what the high-water numbers are for. **Next: rebuild, collect the `stack high-water` line for `import re` (clean), `-c import json` (clean), and interactive `import json` (hangs), then set `PY_UEFI_FIRMWARE_STACK_BUDGET` from the bracket.**
18. **2026-09-08 — SECOND ROOT CAUSE: interactive `exit()` skips the console detach; the budget theory in entry 17 was WRONG.** High-water data @ **`4cf5698a`** (**`entry_rsp = limit + budget = 0x6A951608 + 0x18000 = 0x6A969608`**): **`import re`** → **`min_rsp=0x6A952E68`**, **`used=0x167A0`** (91 808 B = **89.7 KB**), **`0x1860`** = **6 240 B *above* `limit`** so the guard never fired, **clean**; **`-c import json`** → **`min_rsp=0x6A94E9E8`**, **`used=0x1AC20`** (**107.0 KB**), **`0x2C20`** = 11 296 B below **`limit`**, **clean**; **interactive `import json`** → **`min_rsp=0x6A94E8E8`**, **`used=0x1AD20`** (**107.3 KB**), **`0x2D20`** = 11 552 B below, **HANG**. **The clean and hanging runs differ by `0x100` = 256 bytes** — peak depth, overshoot past **`limit`** (~11 KB in both) and **`used`** are all effectively identical, so **stack depth cannot be the differentiator** and entry 17's "96 KB budget is too generous" is **disproved**. **Corollary — do NOT lower `PY_UEFI_FIRMWARE_STACK_BUDGET`:** **`import re`** bottoms out only **6 240 B** above **`limit`**, so the budget is **nearly too tight already**; the change entry 17 proposed would have started raising **`MemoryError`** on shallow, working imports and on Phase 8. (Depths are this large because these are **`-b NOOPT`** builds — no inlining, no frame reuse.) **Actual cause, from diffing the two traces:** the interactive run is **missing `Py_RunMain after pymain_run_python`, `edk2_console_detach_readline enter/leave`, `stop_timer`, `Py_RunMain after Py_FinalizeEx`, `after Py_BytesMain` and `after main()`** — it jumps straight from **`exit()`** to **`Py_FinalizeEx enter`**. **`exit()`** raises **`SystemExit`**, which leaves **`PyRun_InteractiveOne()`** into **`PyErr_Print()`** → **`_Py_HandleSystemExit()`** → **`Py_Exit()`**, and **`Py_Exit()`** calls **`Py_FinalizeEx()`** then **`exit()`**, **never returning through `Py_RunMain()`** — where the only detach call lives (**`Modules/main.c:730`**). So the image exits having **never called `CloseProtocol` for `gEfiSimpleTextInputExProtocolGuid` on `SystemTable->ConsoleInHandle`**, leaving an interface registered against a vanishing image handle for BDS to trip over at Shell **`exit`**. **Explains everything:** **`-c`** never reads interactive input so **ConInEx is never opened** (nothing to leak, which is why **`-c`** is clean either way); **`exit()`** returns to the Shell fine because the leak is a firmware registration, not Python damage; hang only at **`exit`** into BDS; identical depths. **Fix @ this commit:** detach from inside **`Py_FinalizeEx()`** (**`PyMod-3.12.13/Python/pylifecycle.c`**, at the **`Py_FinalizeEx enter`** trace point, plus **`#include "efi/edk2console_api.h"`** under **`UEFI_C_SOURCE`**) so **every** exit route is covered — normal return, **`Py_Exit()`**, and the re-entry cleanup in **`Programs/python.c`**. **`edk2_console_detach_readline()`** is **idempotent** (guards on **`console_in != NULL`**; **`stop_timer`** reports *"already off"*), so the duplicate call on the normal route is harmless. **No INF change needed:** both **`Python312.inf:67`** and **`Python312_MIN.inf:67`** already compile **`efi/src/edk2console.c`**, and **`PyMod-3.12.13/efi/Include`** is on both the MSFT (**`:1073`**) and GCC (**`:1074`**) **`CC_FLAGS`**. **Verify:** interactive **`import json`** → **`exit()`** → Shell **`exit`** must now print **`edk2_console_detach_readline enter/leave`** *inside* the **`Py_FinalizeEx`** block and reach BIOS setup; then re-run **`import re`**, Phase 8 and pyreadline phases 2/3 to prove the extra call is harmless. **Capability gap unchanged** — **`json`**/**`logging`**/pyreadline still raise **`MemoryError`** on VS2022 until the stack switch works under MSVC. Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_FULL_interactive_exit_leak.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_FULL_interactive_exit_leak.md).
19. **2026-09-08 — CORRECTION to entry 18: the leaked-ConInEx mechanism is ruled out; the hang is still OPEN.** Entry 18's *measurement* stands (depth is not the variable; **do not** lower the budget; the exit route is the one real difference) but its *mechanism* does not. **`console_in`** is set in only two places and **neither runs in the reported scenario**: **`edk2main.c:119`** is inside **`#ifdef PY_UEFI_PYREADLINE`** and **`PY_UEFI_PYREADLINE` is not defined in any INF** (it appears only in **`edk2main.c`** and **`Modules/main.c`** — grep confirms no **`.inf`** hit), and **`edk2console.c:128`** (**`edk2_console_ensure_input`**) is reached only from **`edk2console.getkeys()`** at **`:227`**, i.e. the **pyreadline input hook**. The reported session had **`PY_UEFI_READLINE` unset**, so **`readline`** was the **stub**, the REPL was **stdio**, **`getkeys()`** never ran, and **`g_edk2_globals.console_in`** stayed **NULL** — with which **`edk2_console_detach_readline()`** skips **`CloseProtocol`** and **`drain_input`** returns immediately, so **there was nothing to leak and the added detach is a no-op on this path.** **The `pylifecycle.c` change is KEPT as hardening** — it is genuinely required for **`PY_UEFI_PYREADLINE`** development builds and the pyreadline phases, where **`console_in`** *is* non-NULL and **`Py_Exit()`** would really leak it — **but it does not fix this hang.** **Also strengthened:** both **`min_rsp`** values lie inside the **same 4 KB page** (**`0x6A94E000`**), so a page-aligned firmware stack base cannot sit between them, closing the last route by which depth could have mattered. **What remains confounded:** the reported test changed **two** things at once — the **`Py_Exit()`/longjmp exit route** *and* the fact that the session **read interactive stdin** (which no **`-c`** run does). **Decisive test, no rebuild, runs on the `4cf5698a` image:** **`sys.exit()`** inside **`-c`** takes the **same `Py_Exit()` route** — **`PyRun_SimpleStringFlags`** calls **`PyErr_Print()`** on any exception (**`Python/pythonrun.c:508`**), **`_PyErr_PrintEx`** calls **`handle_system_exit`** (**`:773`**), which calls **`Py_Exit(exitcode)`** (**`:777`**) — so **`Python312.efi -S -c "import sys; sys.exit(0)"`** exercises the route **shallow and non-interactively**. **Hangs ⇒ the exit route alone is sufficient** and depth plus interactive stdin are both irrelevant; **clean ⇒ interactive stdin is the variable**, and the follow-up is a shallow interactive session (**`1+1`** then **`exit()`**). **Absence of `after Py_BytesMain`/`after main()` in the trace is the self-verifying marker that the `Py_Exit()` route was taken.** Note **`main()`'s tail is only a trace print and `return rc`** (**`Programs/python.c:39-40`**), so nothing functional is skipped in port code — the difference lives inside the C runtime's **`exit()`**.
20. **2026-09-08 — the exit route is EXONERATED too; the only variable left is "the run read the console".** Hardware @ **`abab8acc`**: **`Python312.efi -S -c "import sys; sys.exit(0)"`** → Shell **`exit`** **CLEAN**, while interactive **`import json`** → **`exit()`** → Shell **`exit`** still **HANGS**. Since **`sys.exit()`** inside **`-c`** takes the *same* **`Py_Exit()`** route (**`pythonrun.c:508/773/777`**), **the route alone is not sufficient.** **Corroborated by `StdLib/LibC/Main/Main.c`:** both routes **converge** — **`exit()`** runs **`exitCleanup()`** (atexit handlers + **`gMD->cleanup`**) then **`_Exit()`**, which **`longjmp`**s to the **`setjmp(gMD->MainExit)`** at **`:191`**, and everything after that block (**`ExitVal = gMD->ExitValue`**, the **`close(i)`** loop over all **`OPEN_MAX`** fds at **`:210-212`**, the **`FreePool(gMD)`** at **`:215-220`**) is **outside** the **`setjmp`** and runs either way. The only difference is the **`longjmp`** itself and the skipped **`after main()`** print — nothing functional. **Elimination table:** depth **NO** (256 B apart, same 4 KB page); exit route **NO** (just tested); leaked ConInEx **NO** (entry 19 — **`console_in`** is NULL for a stdio REPL); **`.pyc`** writes **NO** (entry 14, **`-B`**); descriptor exhaustion **NO** (**`OPEN_MAX 255`**, and StdLib closes all fds on both routes). **Remaining variable: the interactive run READ THE CONSOLE via `PyOS_Readline`.** Every **`-c`** run opens **`stdin:`** as a TTY at **`Main.c:171`** but **never reads it**. **Next tests hold the exit route constant (normal return) and vary only the console read, using `input()` because it goes through `PyOS_Readline` like the REPL:** **T1** **`-S -c "s=input('t: '); print('read', s)"`** (read, shallow); **T2** **`-S -c "s=input('t: '); import json; print('ok')"`** (read, deep); **T3** plain interactive **`1+1`** → **`exit()`** (full REPL, shallow). **T2 hang + T1 clean ⇒ trigger is console read + deep import with the exit route irrelevant — a fully non-interactive repro, the tightest yet. T1 hang ⇒ the console read alone suffices and depth is irrelevant too. T1/T2 clean + T3 hang ⇒ something specific to the REPL loop rather than to reading.** Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_FULL_interactive_exit_leak.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_FULL_interactive_exit_leak.md).
21. **2026-09-08 — MINIMAL REPRO FOUND: `Python312.efi -S` + `import json` + `raise SystemExit` + Shell `exit` = HANG. Requires the REPL AND a deep import; neither alone.** **T1–T5 all CLEAN**, which eliminated a whole family of theories: **T1** **`-c "s=input('t: ')"`** (console read, shallow) clean; **T2** **`-c "s=input('t: '); import json"`** clean and **printed `MemoryError`** — so the guard genuinely fired, making it a valid instance of *overflow after a prior console read*, which exits fine; **T3** plain REPL **`1+1`** → **`exit()`** clean; **T4** **`-c "import re; s=input('t: ')"`** (read *after* a big non-overflowing import) clean; **T5** staged **`t.py`** with **`try: import json / except MemoryError`** then **`input()`** (read *after* the overflow, caught) clean. **So "console read + deep import" is not a sufficient pair, and "read after the overflow" is not the trigger.** **T7 then HUNG under `-S`**, eliminating three more suspects in one shot: **`site` never loaded**, so **`exit`** did not exist, **`site.Quitter.__call__`** never ran and its **`sys.stdin.close()`** never happened, and the extra **`site`** module baseline is absent — **none are involved**. **Full elimination list now:** stack depth (256 B apart, same 4 KB page), the **`Py_Exit()`** exit route (entry 20), leaked ConInEx (entry 19), **`.pyc`** writes (entry 14), descriptor exhaustion, **`site`**/**`exit()`**/**`sys.stdin.close()`**, console read before the overflow, console read after the overflow. **What still differs between T7 (hang) and `-c "import json"` (clean):** (a) the **tokenizer reads the console via `PyOS_Readline`** — twice in T7, never in **`-c`**; (b) **execution continues after the `MemoryError`** instead of exiting immediately; (c) **`sys.last_type`/`last_value`/`last_traceback`** are set by **`PyErr_Print()`** and then **kept alive while more code runs**, pinning the overflow-era frame objects. **T5 does not control for (c)** — it *caught* the exception, so **`PyErr_Print()`** never ran and **`sys.last_*`** were never set; *"uncaught overflow followed by more execution"* is still untested outside the REPL. **Remaining decomposition:** **T6** plain REPL **`import re`** → **`exit()`** (**hang ⇒ the overflow is irrelevant**, it is REPL + big import); **T8** **`-S`** REPL **`exec("try:\n import json\nexcept MemoryError:\n print('caught')")`** → **`raise SystemExit`** (**clean ⇒ the *uncaught* exception / retained `sys.last_*` frames matter, not the import**). **NEW — firmware-side evidence is finally cheap:** entry 14 proposed a **`memmap`** comparison but there was no short repro; T7 provides one, and the prompt returns normally so **`memmap`** runs *before* **`exit`**. Compare free-page totals and descriptor counts at **fresh boot** / after **`-S -c "import re"`** (clean) / after the **T7 sequence** (hanging). **This is the first evidence that does not require guessing the mechanism first** (needs a Debug shell profile). **Doc consequence:** the VS2022 **§4 "REPL and teardown"** sign-off covers **shallow sessions only** — §4 now carries a warning to that effect.
22. **2026-09-08 — TRIGGER ISOLATED TO ONE VARIABLE: an *uncaught* stack-overflow `MemoryError` followed by continued execution.** **T6 CLEAN:** plain REPL **`import re`** → **`exit()`** → Shell **`exit`** no hang. **`re`** is a big import (42 modules, ~90 KB of stack) but stays **under** the bound, so no **`MemoryError`** — therefore **"REPL + big import" is not the trigger; the guard actually has to fire.** **T8 CLEAN:** **`Python312.efi -S`** → **`exec("try:\n import json\nexcept MemoryError:\n print('caught')")`** → **`raise SystemExit`** → Shell **`exit`** no hang. **T8 is the controlled counterpart of T7** — identical REPL, identical **`-S`**, identical deep import, identical continued execution — **differing only in whether the overflow exception is caught, and that single change flips HANG to CLEAN.** **Complete truth table, all four rows now confirmed on hardware:** *(a)* no overflow + continue → **clean** (T6, T4); *(b)* overflow + uncaught + **immediate exit** → **clean** (**`-c "import json"`**); *(c)* overflow + **caught** + continue → **clean** (T5 script, **T8** REPL); *(d)* overflow + uncaught + continue → **HANG** (T7, original report). **Every one- and two-condition combination is clean; only the full triple fails.** **Mechanism this pins:** the difference between *(c)* and *(d)* is precisely what **`PyErr_Print()`** does that a **`try`/`except`** does not — it **displays the traceback** and stores it in **`sys.last_type`/`sys.last_value`/`sys.last_traceback`**, which **pins the overflow-era frame objects** instead of letting them be released, and the interpreter then keeps running with them held. In *(b)* the process exits before that can matter; when caught, they are never pinned at all. **Next — T9, escape the REPL:** stage a script that reproduces **`PyErr_Print()`**'s handling faithfully — **`sys.last_type, sys.last_value, sys.last_traceback = sys.exc_info()`** then **`sys.excepthook(...)`**, then **`input()`** and continue. **A hang removes the REPL from the repro entirely**, turning this into an unattended scripted test fit for the smoke doc. **If T9 hangs, split it:** drop the **`sys.excepthook(...)`** call to test **retention alone**, or keep it and **`del`** the three **`sys.last_*`** names afterwards to test **traceback printing alone**. Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_FULL_interactive_exit_leak.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_FULL_interactive_exit_leak.md).
23. **2026-09-08 — T9/T10 CLEAN, and then the ROOT ENABLER FOUND: the VS2022 stack switch never worked because the NASM uses the System V ABI.** **T9 CLEAN** (and it **did** print **`MemoryError`** plus the full traceback, so its row is valid): reproducing **`PyErr_Print()`**'s handling in **`-c`** — traceback displayed *and* pinned in **`sys.last_type`/`last_value`/`last_traceback`** — then continuing to run, exits fine. **T10 CLEAN:** T9 plus a leading **`input()`**, i.e. console reads straddling an uncaught printed overflow, also exits fine (**`min_rsp=6A950CE8 limit=6A951608 used=18920`** — 98.3 KB peak, guard fired 2 336 B below **`limit`**). **So the sequence-of-operations model is dead too: the REPL is structurally required, and ten Python-level tests have now exhausted what can be learned from Python.** **Pivot to the enabler — and the answer was in hand-written assembly.** **`edk2_switch_stack`** (**`efi/src/edk2stack.nasm:13-23`**) reads its arguments from **`rdi`/`rsi`** — **System V** — but its C prototype is a **plain** function, not **`EFIAPI`**, so **MSVC passes them in `rcx`/`rdx`**. Under MSVC **`add rdi, rsi`** sums two unrelated live registers and **`lea rsp, [rdi-0x208]`** points **`rsp`** at an arbitrary address; the following **`ret`** then reads a return address from there. **A hang inside `ShellCEntryLib` is the only possible outcome — exactly what `edk2main.c:203-205` and `Python312.inf:1070-1072` recorded, and it was never a firmware incompatibility.** **Same defect in the IDT helpers:** **`edk2_get_idtr`** does **`sidt [rdi+6]`** and **`edk2_set_idtr`** does **`lidt [rdi+6]`** (**`edk2handler.nasm:319-330`**), so under MSVC both operate through a garbage pointer — **which is why the stack switch and the IDT install appeared to fail *as a pair*: one root cause, and the comments blaming them jointly were describing a symptom.** **Unaffected:** **`edk2_revert_stack`**, **`edk2_read_rsp`**, **`edk2_pause`** take **no arguments**, which is why **`edk2_read_rsp()`** worked on VS2022 and the **`stack_limit`**/**`stack_min_rsp`** instrumentation read correctly throughout; **`py_common_interrupt_entry`** is also fine, already loading **`rcx`/`rdx`** per the EDK2 x64 convention before calling **`py_handle_exception`**. **This closes the causal chain for the whole VS2022 bug family:** broken NASM ABI → **`PY_UEFI_MSVC_368_ENTRY`** workaround → interpreter runs on the **~128 KB firmware stack** instead of **64 MB** → deep imports overflow → the overflow lands in **live Shell/BDS memory** (hence flawless Python teardown and a hang only at Shell **`exit`**) → **`json`**/**`logging`**/pyreadline unusable. **Fix:** ABI-aware NASM via **`%ifdef PY_UEFI_MS_ABI`** → **`ARG1`/`ARG2`** = **`rcx`/`rdx`** else **`rdi`/`rsi`**, with **`MSFT:*_*_*_NASM_FLAGS = -DPY_UEFI_MS_ABI`** added to **both** INFs — **GCC codegen is byte-for-byte unchanged, so its sign-off stands.** **The ABI fix is inert in the current default build** (with 368 still in force none of the three functions are called on the MSVC path), so it carries **no risk on its own**; the behaviour change is entirely behind the new opt-in **`PY_UEFI_MSVC_STACK_SWITCH`**, which takes the 64 MB path on MSVC and skips **`py_install_idt()`**/**`py_restore_idt()`** unless **`PY_UEFI_MSVC_IDT`** is also defined, so stack and IDT can be brought up one at a time. **Also fixed for MSVC only: stack alignment.** **`edk2_switch_stack()`** leaves **`rsp`** at **`base+size-0x200`**, so **`base+size`** must be 16-byte aligned or MSVC's **`movaps`** spills fault — and the existing **`aligned_stack = stack + (stack % 512)`** **does not align anything**, it offsets by an arbitrary amount. MSVC now uses **`(base + 511) & ~511`** (safe: the **`malloc`** over-allocates by 1024). **The GCC expression is deliberately left wrong-but-untouched** pending its own re-test. **To try it:** add **`/DPY_UEFI_MSVC_STACK_SWITCH=1`** to the MSFT **`CC_FLAGS`** in **`Python312.inf:1073`** and rebuild. **Trace tells you the path immediately:** **`firmware stack rsp=...`** should be **absent**, replaced by **`before switch_stack`** → **`skipping py_install_idt (MSVC stack-switch opt-in)`** → **`before ShellCEntryLib`**; a stop at **`before ShellCEntryLib`** means the switch still fails and the flag should be reverted. **Acceptance:** **`-S -c "import json; print('ok')"`** printing **`ok`** with **no `MemoryError`** is the clearest signal — impossible on VS2022 for the entire port — plus **`import logging`**, the T7 sequence exiting cleanly, an unchanged Phase 8 sweep, and pyreadline §5.3/§5.4 finally working. **If it works, these become obsolete:** **`PY_UEFI_MSVC_368_ENTRY`** for X64 MSVC, **`PY_UEFI_FIRMWARE_STACK_BUDGET`** on that path, and the *"`MemoryError: stack overflow` is expected"* rows in the smoke doc. Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_nasm_abi_mismatch.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_nasm_abi_mismatch.md).
24. **2026-09-08 — VS2022 NOW RUNS ON THE 64 MB STACK. `import json` works and `exit` is clean — a first for this port.** Two defects, both required. **Attempt 1 on hardware** printed **`before switch_stack`** → **`skipping py_install_idt`** → **`before ShellCEntryLib`** then **stuck** — and *those two lines printing after the switch confirmed the ABI fix*, since with garbage **`rsp`** the **`ret`** inside **`edk2_switch_stack`** could not have returned at all. **Defect 2: `edk2_switch_stack()` moves `rsp` out from under a *running* function.** **`UefiMain`**'s prologue has already run, so its locals and spilled parameters live in a frame on the **old** stack. **GCC at `-O0` keeps a frame pointer**, so those accesses go through **`rbp`** into the old stack and keep working — which is why this was never noticed on the signed-off toolchain. **MSVC x64 uses a fixed frame with `rsp`-relative addressing and no frame pointer**, so **`image`**, **`systab`** and **`status`** resolved into the *new* stack at the old offsets. The **`PY312_BOOT_PRINT`** lines survived because they pass only **string literals**; the very next statement, **`ShellCEntryLib(image, systab)`**, read two broken parameters and handed **`ShellCEntryLib`** garbage handles. **Fix:** read the handles from **`g_edk2_globals`** (populated well before the switch, RIP-relative, immune to **`rsp`**) and park the result in a new **`g_edk2_globals.switch_status`** until **`edk2_revert_stack()`** makes the frame addressable again. **Sufficient, not a patch:** between switch and revert **`UefiMain`** executes only **`PY312_BOOT_PRINT`** (literals), the skipped IDT calls, the **`ShellCEntryLib`** call, and **`edk2_revert_stack()`** (no args). **Attempt 2 result — PASS:** **`Python312.efi -S -c "import json; print('ok')"`** → **`ok`**, Shell **`exit`** **clean**. **`import json` has never succeeded on VS2022** — it raised **`MemoryError: stack overflow`** after the **`PyOS_CheckStack`** fix, and silently corrupted firmware memory before it. **The root enabler of the entire VS2022 bug family is fixed.** **Added:** a **`switched stack min_rsp=... limit=... size=...`** boot-trace line for the switch path (the old **`stack high-water`** print existed only in the 368 branch), emitted **after** the revert so the frame is valid under MSVC; **`used`** should be a small fraction of **`0x4000000`** and **`min_rsp`** far above **`limit`**, else 64 MB is not really in play. **THE ORIGINAL DEFECT IS ALSO FIXED:** the interactive route — **`Python312.efi`**, **`import json`** (**no `MemoryError` at all`**), **`exit()`**, Shell **`exit`** — is **clean**. That sequence hung on every prior attempt and survived ten rounds of Python-level bisection (**T1–T10**) in **`2026-09-08_VS2022_FULL_interactive_exit_leak.md`**, now closed. That note's final model — *uncaught stack-overflow **`MemoryError`**, traceback printed and retained in **`sys.last_*`**, execution continuing, REPL structurally required* — was an accurate description of the **trigger** and a wrong theory of the **cause**: each condition was merely a route to a depth the ~128 KB firmware stack could not hold, and the "REPL requirement" was really *"the process stayed alive long enough to touch the corrupted firmware memory again"*, which **`-c`** and script routes did not. Note the contrast with the earlier **`PyOS_CheckStack`** fix, which made the overflow **visible and survivable**; this one means **there is no overflow to detect**. **Method lesson:** the Python-level search space was exhausted — T1–T10 all resolved exactly as predicted — while the cause sat one layer below in assembly *never exercised on this toolchain*; the clue that should have redirected the search far sooner was the early observation that **GCC never hung**, which pointed at toolchain divergence rather than at interpreter behaviour. **FULL ACCEPTANCE SWEEP GREEN — the switch is now the DEFAULT for MSVC.** **`import logging`**, the **Phase 8** §3 sweep, and **pyreadline §5.3 / §5.4** (**`Readline True`**, up-arrow history and Tab completion) all pass with clean **`exit`**. **The retired configuration is bit-identical to what was signed off:** the old guard was **`#if defined(PY_UEFI_MSVC_368_ENTRY) && !defined(PY_UEFI_MSVC_STACK_SWITCH)`** and the validated build defined **both**, so the 368 branch was already dead code in the tested image — removing it changes no generated code. **Removed:** **`PY_UEFI_MSVC_368_ENTRY`** (both INFs + its branch), the **`PY_UEFI_MSVC_STACK_SWITCH`** gate (MSVC-specific pieces now key on plain **`_MSC_VER`**), **`PY_UEFI_FIRMWARE_STACK_BUDGET`** (dead — **`stack_limit`** derives from the real allocated base), **`g_edk2_globals.stack_entry_rsp`**, and the stale doc claims: the *"`MemoryError` is expected"* rows, the §4 shallow-sessions-only warning, and the firmware-stack deviation in **`..._Toolchain_Deviations.md`** §11.1. **Docs corrected rather than deleted** — the runtime notes' *"do not drop 368 on FULL"* and *"re-add 368 if boot hangs"* instructions were actively wrong and now point at **`PY_UEFI_MS_ABI`** instead. **STILL OPEN:** **(a) MIN is untested on this path** — it carried 368 too, so it moved onto the switched stack without a hardware run; only FULL was swept, flagged in **`Python312_VS2022_MIN_Build.md`**. **(b) The IDT is still skipped under MSVC** (**`PY_UEFI_MSVC_IDT`** opts in); the **`idtr`** ABI is fixed so it may now work and would restore fault reporting, but the sweep was signed off with it off. **(c) The GCC alignment expression `stack + (stack % 512)` is still wrong** — it offsets rather than aligns, and needs the same round-up plus a GCC re-test. **(d) `edk2_alloc_environ()` is called twice** (`:200`, `:209`) — pre-existing on GCC, and MSVC now runs both where the early return used to skip the second; left alone since both signed-off images are built that way. **REBUILT AND FULLY RE-SWEPT — SIGNED OFF, tag `python312-vs2022-full-64mb-stack-2026-09-08`.** Rebuilt VS2022 FULL **`-b NOOPT`** at **`32c63ba1`**: clean, **0 MSVC errors**, 2 min 40 s. Retirement verified against the **image** rather than the source, by searching it for UTF-16 literals — **`switched stack`** and **`skipping py_install_idt`** present, **`368-style`** and **`firmware stack rsp`** gone. **Correction to an earlier claim in this entry:** removing the branch is *"same code path"*, **not** *"same bytes"* — the rebuilt image is **2 560 bytes smaller** (dropping **`stack_entry_rsp`** shrinks **`edk2_globals_t`**, the new trace string is added, and **`/ALIGN:4096`** shifts padding), so the rebuild validates it rather than the argument. **Every smoke test re-run on the new image passes:** **`import json`** / **`import logging`** on **`-c`**; the REPL deep-import repro with both **`raise SystemExit`** and **`exit()`**; **§3 Phase 8** incl. **`ctypes.sizeof(c_void_p)`** → **`8`** and **`ssl.create_default_context()`**; **§4** REPL + relaunch; **pyreadline §5.3 / §5.4** with history and Tab. **No `MemoryError` anywhere.** **THE STACK MEASUREMENT, first ever read:** **`switched stack min_rsp=6486B0A8 limit=60877038 size=4000000`** → base **`0x60875038`**, depth used **`0x9D90` = 40 336 B ≈ 39.4 KB**, i.e. **0.06 % of 64 MB** with **63.95 MB** headroom. **`size` is confirmed independently** of the screen-truncated field, since **`min_rsp`** is a real observed **`rsp`** sitting 63.95 MB above **`limit`**. **This is the quantitative proof of the root cause:** **`import sys`** — the *shallowest* useful run — needs **41 % of the retired 96 KB firmware budget on its own**, and **`import re`** was previously measured at ~90 KB with only 6 240 B of margin, so a deeper import on the firmware stack was **arithmetically doomed** and **no budget tuning could ever have fixed it**; the earlier note that 96 KB was *nearly too tight rather than too generous* was right. **Two process failures of mine, both fixed:** *(i)* **`git add -A`** in **`d71094a2`** committed **278 files** that were untracked for good reason — all byte-identical **`srcprep.py`** duplicates of **`PyMod-3.12.13`** originals (**`efi/`** entirely, **`Modules/openssl/`**, **`Modules/zlib/`**, **`Lib/ssl/`**, two **`Tools`** wrappers). That commit's real content is its **14 modified** files; all 278 strays were pure additions, so the split was unambiguous. Untracked in **`6ebb5321`** (files stay on disk; builds unaffected) and **`.gitignore`** extended to cover the rest of what srcprep writes — otherwise a plain **`srcprep.py`** run shows as 278 modifications and the tracked copies can drift from their sources. *(ii)* **§5.3's `'edk2console' in sys.modules` check was unreliable**, returning **`False`** while the list form showed both **`edk2console`** and **`pyreadline.console.edk2`** loaded and **`disable_readline`** was **`False`**; likely the nested single quotes in the **`-c`** string, root cause unconfirmed. **§5.2 mattered more** — there the expected answer is *negative*, so a wrongly-**`False`** boolean would convert a real failure into a **silent pass**. Both now use **`disable_readline`** / the list form. **Incidental, harmless:** **`edk2_console_detach_readline`** runs **twice** (from **`Modules/main.c`** and again from **`Py_FinalizeEx()`** per **`abab8acc`**), both reporting **`stop_timer: already off`** — idempotent, but redundant now that the real cause is known. ~~STILL OPT-IN pending the rest of the sweep:~~ **`import logging`**; **Phase 8** §3; **§4 REPL + relaunch**; **pyreadline §5.3/§5.4** (expect **`Readline True`** and working interactive editing). **Retirement list once it passes, in order:** *(1)* **`PY_UEFI_MSVC_368_ENTRY`** from both INFs and its branch in **`edk2main.c`**; *(2)* **`PY_UEFI_FIRMWARE_STACK_BUDGET`**, dead once **`stack_limit`** derives from the real allocated base instead of a budget measured down from entry **`rsp`**; *(3)* the *"`MemoryError: stack overflow` is expected"* rows in smoke §6; *(4)* the §4 shallow-sessions-only warning; *(5)* the firmware-stack-vs-64 MB deviation in **`Python312_VS2022_GCC_Toolchain_Deviations.md`** §11.1 — both toolchains would finally share one entry path. **Separate follow-ups, deliberately not bundled:** **`py_install_idt()`** is still skipped here (**`PY_UEFI_MSVC_IDT`** re-enables it) and may now work given the corrected **`idtr`** helper ABI, which would restore fault reporting on VS2022; and the **GCC** alignment expression **`stack + (stack % 512)`** is still wrong and should become the same round-up with its own GCC re-test.
25. **2026-09-08 — GCC PARITY GREEN: the shared entry path is now signed off on both toolchains.** Clean-tree **GCC FULL** rebuild at **`9db93ae1`** (**`-b NOOPT`**, `Build/AppPkg/NOOPT_GCC` deleted first), then the full sweep on hardware: **§3 Phase 8** including **`ctypes.sizeof(c_void_p)`** → **`8`** and **`ssl.create_default_context()`**; the **`json`** / **`logging`** deep imports on **`-c`**; the **`sys.modules`** counts **23 / 42 / 48 / 65**; the **§4** REPL with both **`exit()`** and **`raise SystemExit`**; **pyreadline §5.3 / §5.4**. **Every result matched the VS2022 numbers exactly, no `MemoryError` anywhere, every Shell `exit` clean.** **The inversion worth recording: the risk in this build was not in any MSVC-specific code — it was that fixing MSVC changed GCC.** Three deltas reached GCC-compiled code since the **`python312-unified-full-lab-2026-09-04`** sign-off. **(1) `PyOS_CheckStack()` was rewritten**, from **`rsp > base ? 0 : 1`** — which only reported overflow once **`rsp`** reached the very bottom of the allocation — to **`rsp <= base + PY_UEFI_STACK_MARGIN`**, so **GCC's guard now fires 8 KB earlier**, with room to raise and unwind. **(2) `stack_limit` is now non-zero while Python runs** (set after the switch, cleared after the revert); the field was unused on this path before. **(3) `edk2_globals_t` changed shape** — **`stack_entry_rsp`** out, **`switch_status`** and **`stack_min_rsp`** in — **which is why the clean rebuild was mandatory**: a stale object compiled against the old header reads moved field offsets, and that would have presented as random stack corruption rather than as a build problem. **What was *not* under test on GCC, and why looking for it would have wasted a cycle:** **`UEFI_C_SOURCE`** and **`PY_UEFI_BOOT_TRACE`** are both defined **only** on the **`MSFT:`** flags line, so the **`Py_FinalizeEx()`** detach added in **`abab8acc`** is **not compiled on GCC** and a stock GCC image **prints no boot trace at all** — there is no **`switched stack`** line to read. **`MSFT:*_*_*_NASM_FLAGS = -DPY_UEFI_MS_ABI`** is toolchain-scoped, and the deleted 368 branch was **`_MSC_VER`**-only, so GCC codegen for the NASM and the entry gate is untouched. **Consequence: there is still no GCC stack-depth number** — the optional **`-DPY_UEFI_BOOT_TRACE=1`** throwaway build was skipped, so the 39.4 KB measurement remains VS2022-only. **This also means latent defect §11.8 #1 is not exonerated:** GCC passing proves only that **`malloc`** returned a base which the mis-derived **`stack + (stack % 512)`** offset left usable — which has always been true, and is not the same as the expression being correct. **Closes V4.7** (GCC regression after INF edits). **Still open:** **MIN on either toolchain**, and §11.8 defects **#1** (GCC alignment), **#2** (double **`edk2_alloc_environ()`**), **#3** (IDT skipped under MSVC). **Tag: `python312-gcc-full-parity-2026-09-08`.**
26. **2026-09-08 — §11.8 defect 3 ANSWERED: the custom IDT works under MSVC, and fault reporting on VS2022 is now a decision rather than an unknown.** Built VS2022 FULL with **`/DPY_UEFI_MSVC_IDT=1`** and swept it. **Install works:** trace read **`before py_install_idt`** → **`before ShellCEntryLib`** → normal boot, so **`sidt`**, the 4 KB IDT copy and **`lidt`** all succeeded — the **`PY_UEFI_MS_ABI`** fix that unblocked the stack switch covered the **`idtr`** helpers too, exactly as predicted in item 23. **Restore works and nothing regressed:** the full §3/§4/§5 sweep passed clean and reached the **`switched stack`** line, which is what exercises **`py_restore_idt()`** — the fault test itself never returns, so a clean normal run is the only evidence available for the restore path. **Fault reporting works:** **`ctypes.cast(0x800000000000, POINTER(c_int))[0]`** — non-canonical, so a **#GP** regardless of firmware page mapping — printed **`Python312 boot: unhandled CPU exception 13`**. **That is proof, not inference:** the string lives in **`py_handle_exception()`** and is reachable *only* through a trampoline written by **`py_install_idt()`**, so on the default image it could never have printed; and **13** was the predicted vector, confirming routing as well as install. **Pre-hardware check worth reusing:** the flag was verified against the *image* before deployment by decoding it as UTF-16 and asserting **`skipping py_install_idt`** absent / **`before py_install_idt`** present — the two arms of the **`#if`**, so absence of one is direct evidence the flag compiled, not a guess. **The port was already MSVC-ready and nobody had noticed:** **`edk2excep.h`** carries a **`#pragma pack(push, 1)`** struct byte-equivalent to the GCC bitfield one (both 16 bytes, identical offsets), and **`py_install_idt()`** copies the firmware's live IDT and patches **only the three offset fields**, so type/DPL/present come from the firmware's own descriptors and the packed-vs-bitfield difference never has to generate them; the trampoline is hand-assembled bytes and toolchain-neutral. **Left OFF by default, as a policy call:** **`edk2_seh_try()`/`edk2_seh_catch()`** have **no callers anywhere in the tree**, so **`g_context_index`** is always **`-1`** and **`py_handle_exception()`** always takes the unhandled branch — print, **`raise(signum)`**, then **`while (exc_trap)`** **spins forever**. Enabling the IDT trades a fault the firmware would report and usually **reset** for one useful line plus a **hang until power-cycle**, which for a manufacturing image is arguably worse. **The INF edit was reverted** so the tagged default is untouched; re-enabling is one token. **Making faults survivable is separate work** — it means wiring up the dead **`edk2_seh_*`** path. **§11.8 now stands at: #1 cosmetic (cannot fault), #2 a real per-run pool leak, #3 answered.**
27. **2026-09-08 — §11.8 #2 fixed, #3 and #4 closed: fault reporting is now identical on both toolchains.** **#2 — the `environ` leak is fixed and GCC-verified.** `edk2_alloc_environ()` now frees any previous block on entry, plus a NULL check on the `malloc` before the `memset`. Fixing the **class** rather than the instance keeps any future double call safe and preserves which block wins and when it is allocated, so the end state matches the signed-off images minus the leak; the redundant first call site stays, with a comment, since both signed-off images are built that way. **Safe only because of read ordering** — `environ` is consumed by `posixmodule.c`'s `convertenviron()`, which runs long after the second call and **copies** the strings rather than retaining pointers, and `edk2main.c` is the only caller of either function. **GCC swept clean; VS2022 pending.** **#4 — NEW DEFECT, found while testing #2 on GCC, and it had been live on the signed-off toolchain the whole time.** The §E fault one-liner printed **nothing** on GCC and **silently hung**, where VS2022 with the IDT printed **`unhandled CPU exception 13`** on the identical input. Cause: the handler's only output sat behind **`#ifdef PY_UEFI_BOOT_TRACE`** (`edk2excep.c:92`), which is defined **only** on the **`MSFT:`** flags line — so GCC caught faults, said nothing, and spun in **`while (exc_trap)`** forever. **Silent-hang-with-no-firmware-output is the positive signal**, since a firmware-handled fault prints its own dump; same input, same handler, same spin, and the *only* difference between toolchains was whether one **`Print`** was compiled in. **That is strictly worse than not installing the IDT:** a fault the firmware would report and usually reset on became a silent hang, with the one diagnostic that would justify the trade compiled out. **Fix:** the **`Print`** is now unconditional — *a fault report is not debug tracing, and the spin after it never returns*. It also now carries **`rip`** and **`cr2`**, which is the actual point of reporting: **`unhandled CPU exception 13 rip=<addr> cr2=<addr>`**. Dereferencing **`SystemContext`** is safe because **`edk2handler.nasm`** is a port of EDK2's own **`ExceptionHandlerAsm.nasm`** and passes a fully built **`EFI_SYSTEM_CONTEXT_X64`** (**`mov rcx, [rbp + 8]`** / **`mov rdx, rsp`**, MS x64); **`cr2`** is the faulting address for a page fault and stale otherwise, **`#GP`** included. **#3 — the IDT is now the DEFAULT under MSVC**, via **`/DPY_UEFI_MSVC_IDT=1`** in **both** INFs. Item 26 had left it off because reporting-then-hanging looked like a worse trade than deferring to firmware; **what changed the decision is that GCC has always behaved exactly this way**, so MSVC deferring to firmware was not a safer default, it was a *second* fault behaviour to reason about. With #4 fixed both toolchains now report vector/`rip`/`cr2` and then spin, identically — and the flag is in both INFs specifically so the MSVC entry path cannot diverge again the way **`PY_UEFI_MSVC_368_ENTRY`** did. **The macro's scope is the pattern worth watching:** this is the **second** time **`MSFT`**-only **`PY_UEFI_BOOT_TRACE`** hid something from GCC, after the **`switched stack`** measurement — anything that is *evidence* rather than *tracing* does not belong behind it. **Untested, needs a sweep before the next tag:** VS2022 FULL (all three changes), and **MIN on either toolchain** now carries the IDT flag and the environ fix on top of its existing switched-stack debt. **§11.8 now stands at #1 alone** — cosmetic, proven unable to fault.
28. **2026-09-08 — BOTH TOOLCHAINS GREEN at `3ec592e1`; §11.8 is down to #1. Tag `python312-both-toolchains-idt-fault-report-2026-09-08`.** GCC and VS2022 FULL both rebuilt and swept clean with all three changes in: the **`environ`** leak fix, **unconditional fault reporting** with **`rip`**/**`cr2`**, and the **MSVC IDT on by default**. **The confirmation that counted was GCC's**, not VS2022's: VS2022 had the report line all along, so only GCC could show that #4's fix changed anything — the identical one-liner that **hung silently** before now prints **`unhandled CPU exception 13 rip=… cr2=…`**. GCC images had been swallowing CPU faults for the entire port, on the *signed-off* toolchain, and the sweeps never noticed because a fault was never provoked. **VS2022 side confirmed the other half:** trace read **`before py_install_idt`** (not **`skipping`**), the sweep was unaffected, and the fault line now carries the address. **`import os; print(len(os.environ))`** was added on both as the direct check that the surviving **`environ`** block still populates — the leak itself is not observable from Python, so "nothing regressed" is the only available assertion. **Procedure captured where people will look:** the fault-injection test is now **`Python312_Smoke_Tests.md` §5.8** with expected output and a four-row result table separating *pass*, *reported-but-gated* (the old GCC behaviour), *bypassed-our-handler* (IDT missing), and *the address got mapped, so the test proved nothing* — previously it existed only in §11.8 prose and a chat log. **Method note worth keeping:** #4 was found only because a fix for an unrelated defect (#2) sent us back to the same test on the other toolchain. Neither defect was in the other's causal chain; the pairing was luck. The general lesson is the one from §11.8's NASM audit restated — **a diagnostic that has never been provoked is not evidence of anything**, and **`MSFT`**-only **`PY_UEFI_BOOT_TRACE`** had by then hidden two separate things from GCC. **Open:** §11.8 **#1** (GCC alignment, cosmetic, cannot fault); **MIN on either toolchain**, which now carries the IDT flag and the environ fix on top of its pre-existing switched-stack debt; and, as genuinely new work rather than cleanup, wiring up the dead **`edk2_seh_*`** path so faults become **survivable** instead of reported-then-spinning.
29. **2026-09-09 — MIN VS2022 SWEPT AND SIGNED OFF: no unvalidated configuration remains on this toolchain.** MIN had silently accumulated **four** deltas that were only ever exercised on FULL — **`PY_UEFI_MSVC_368_ENTRY`** removed (so MIN moved onto the 64 MB switched stack), **`/DPY_UEFI_MSVC_IDT=1`** added, the **`edk2_alloc_environ()`** leak fix, and the two new **`edk2_seh_*`** defect fixes below. **All four are now demonstrated, not inferred** ([`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) **§7.1**): boot trace **`size=4000000`** (64 MB — the switched path is genuinely in play, not the ~128 KB firmware stack), **`before py_install_idt`** and *not* **`skipping py_install_idt (MSVC)`**, **`-h`** and **`sys.version`** → **`3.12.13`**, **`import os, sys, json`** plus a **`hashlib`** digest (builtin hashes intact), and **`import ssl`** / **`import ctypes`** failing on **`_ssl`** / **`_ctypes`**. **The §4 row is the one that mattered:** **`-S`** → **`import json`** → **`exit()`** → Shell **`exit`** **clean, no `MemoryError`** — the exact sequence that hung FULL before the 64 MB switch. **Depth measured at ~71 KB of 64 MB (0.11%)** from **`min_rsp=648B71B8`** against **`limit=608CB038`** (base **`0x608C9038`**, top **`0x648C9038`**); higher than FULL's 39.4 KB only because the sampled run was formatting an **`ImportError`** traceback. **`min_rsp=0` on the `-h` run is not a defect** — **`PyOS_CheckStack()`** is reached only from **`PyObject_Repr`**, **`PyObject_Str`** and **`_Py_CheckRecursiveCall`**, and **`-h`** prints usage from C without evaluating Python, so it samples nothing; **`edk2main.h:62`** documents zero as "never sampled". **§5.8 is n/a on MIN** — there is no **`ctypes`** to build the bad pointer with, so the IDT rests on the trace line instead. **Two real defects were found in the `edk2_seh_*` path while designing fault recovery, and fixed here** ([`Python312_SEH_Fault_Recovery_Design.md`](./Python312_SEH_Fault_Recovery_Design.md) §2): **(1)** **`py_common_interrupt_entry`** starts with **`cli`** and it is the **`iretq`** on the normal return path that restores **`RFLAGS.IF`** — the recovery **`longjmp`** never reaches it, so a recovered fault would have left **interrupts masked for the rest of the run** and stopped the firmware timer **`edk2console`** depends on; now conditionally re-enabled from the faulting context's own **`Rflags & BIT9`**, since a fault inside a critical section must not resume with interrupts on. **(2)** **`edk2_seh_try()`** tested **`++g_context_index >= EDK2_SEH_CONTEXT_SIZE`** *after* incrementing and returned **`NULL`** without undoing it — and the caller that got **`NULL`** never calls **`edk2_seh_catch()`**, the only decrementer, so the index stuck past the end and **the next fault would write a whole `EFI_SYSTEM_CONTEXT_X64` out of bounds and `longjmp` through it**. **Both were latent purely because nothing in the tree calls `edk2_seh_try()`** — the mechanism is complete and has never had a caller. Verified **not** a problem while reading it: x64 **`BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT`** is **8** and **`ret_context`** sits at offset 8, so **`SetJump`** cannot fault inside the fault handler; and **`LongJump`** restores **`MxCsr`**/**`XMM6-15`** itself, so skipping the handler's **`fxrstor`** loses nothing the ABI requires. **The build also settled an open question:** **`EnableInterrupts()`** resolves without adding **`BaseLib`** to **`[LibraryClasses]`**, being already in the module's link closure via **`UefiLib`**/**`DebugLib`**. **The recovery API itself is designed but NOT implemented** — deliberately narrow (**`uefi.mem_read`** / **`mem_write`** / **`mem_probe`** with a **`FaultError`** carrying **`vector`**/**`rip`**/**`cr2`**, hosted on the existing builtin **`uefi`** module so no **`config.c`** or INF plumbing is needed). **A general "guard any Python callable" was rejected on principle:** a **`longjmp`** out of a fault deep inside CPython discards every intervening C frame with no unwinding — no **`DECREF`**s, nothing freed, possibly a half-mutated container or a held internal lock — so only leaf code holding no Python references can safely sit inside the guarded region.
30. **2026-09-09 — GCC MIN swept for the first time in this port; the `EnableInterrupts()` link risk is cleared on both toolchains.** **§2 and §4 green** on GCC MIN ([`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) **§7.2**) — it is a deliverable and had **never** been run, the GCC line having always been validated on FULL. **The cross-toolchain result that mattered was the build, not the sweep:** the two **`edk2_seh_*`** fixes add **`EnableInterrupts()`**, and their only real risk was needing an explicit **`BaseLib`** in **`[LibraryClasses]`**; GCC linking cleanly proves **`BaseLib`** sits in the module's closure via **`UefiLib`**/**`DebugLib`** on **both** toolchains. Also confirmed while checking this: both INFs list **`PyMod-3.12.13/efi/src/edk2excep.c`** directly and **`srcprep.py`** does not touch the **`efi`** tree, so a plain rebuild picks the edit up — the VS2022 MIN sign-off was not a build against a stale copy. **One honest gap recorded rather than papered over:** GCC prints **no boot trace** (`PY_UEFI_BOOT_TRACE` is `MSFT:`-only) and **§5.8 is n/a on MIN** (no **`_ctypes`**, and pure Python cannot dereference an arbitrary address), so **GCC MIN is the single configuration where neither the IDT install nor fault routing is directly observable**. Unclosable until the guarded primitives exist — which is now noted in the design doc as a side benefit of hosting them on the always-built **`uefi`** module. **Still outstanding: GCC FULL including §5.8**, the run that exercises **`py_handle_exception()`** itself. Tag deliberately **held** until then; at this moment the fixes are hardware-verified on VS2022 MIN and compile-verified on GCC, with GCC fault reporting not yet re-confirmed since they landed.
31. **2026-09-09 — GCC FULL green including §5.8; three of four configurations swept at this code state.** **§2 + the `os.environ` canary + §3 Phase 8 (`ctypes.sizeof(c_void_p)` → `8`, `ssl.create_default_context()` → `ok`, `phase8 ok`) + §4 REPL + §5.8** all pass on GCC FULL ([`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) **§7.3**). **§5.8 is the row that mattered:** the **`edk2_seh_*`** fixes edited **`py_handle_exception()`**, and GCC is the toolchain whose **`Print`** in that very function was silently compiled out until 2026-09-08 (deviations §11.8 #4) — so a green **`unhandled CPU exception 13 rip=… cr2=…`** confirms the reporting path survived the change where it was historically fragile. **Matrix now: VS2022 MIN, GCC MIN and GCC FULL swept; VS2022 FULL not rebuilt since the fixes landed** (last swept at **`3ec592e1`**). **That gap is small and is being stated, not assumed:** **`edk2excep.c`** compiles into both VS2022 INFs off the *same* **`MSFT:`** **`CC_FLAGS`** and VS2022 MIN has executed it end to end, so compile/link/run are covered under MSVC; the modified lines sit in the **dead** recovery path, the *unhandled* branch that §5.8 exercises was **not touched**, and GCC FULL has now confirmed that branch with the fixes in. Residual risk is only that FULL links more modules, which cannot affect this file — but a VS2022 FULL rebuild plus §5.8 closes it outright and should precede any release shipping FULL on VS2022.
8. **Next diagnostic (no rebuild needed):** **`/DPY_UEFI_BOOT_TRACE=1`** is already on MSFT **`CC_FLAGS`**, so **`PY312_CONSOLE_TRACE`** lines (**`edk2_console_detach_readline enter/leave`**, **`stop_timer: …`**, **`handoff_to_shell`**) are live in the tested image — read them on a phase 2 re-run before typing **`exit`** to decide whether detach ran and completed. **Do not** try a ConIn `Reset`; **`edk2console.c`** records both directions already failing.
9. **Policy unchanged:** VS2022 manufacturing stays **stdio**; this run is positive evidence for that decision.

### 2026-09-04 — unified FULL (VS2022 + GCC) post-PyMod regression + smoke doc

1. **Hardware (both toolchains, FULL, `3afa03f5`, NOOPT):** **`sys.version`**; **`import zlib, ctypes, hashlib`**; **`import ssl; ssl.create_default_context()`** — each followed by Shell **`exit`** → firmware, plus relaunch without reboot — **pass**, no hang, on **VS2022 and GCC**. Stdio REPL default (no **`PY_UEFI_READLINE`**) on both.
2. **Same code state:** Windows clone and WSL clone (`/home/jp/src/edk2-libc-jp-vsfix`) both at **`b60db389`**; commits after **`3afa03f5`** are docs-only. Each deployed **`.efi`** verified byte-identical to its build output by hash; GCC image distinct from the 2026-09-01 sign-off build.
3. **Significance:** first hardware run on **either** toolchain after **`9d465ec2`** (PyMod consolidation), **`55219522`** (frozen headers under PyMod) and **`0a674ac0`** (deepfreeze helper path fix) — relocated frozen/deepfreeze artifacts confirmed good on **both** MSVC 368 and GCC `edk2_switch_stack` entry paths. Note the 2026-09-01 unified tag **predates** PyMod consolidation.
4. **Gaps closed same day:** **`ctypes.sizeof(c_void_p)`** → **`8`** on both (LLP64 / **`UEFI_MSVC_64`** canary — a **`4`** would be the `libffi_msvc` `ffi_type_pointer` bug, deviations §2.2), and **`import zlib, ssl, ctypes, hashlib`** → **`phase8 ok`** in a single process on both, teardown clean. Every check guarding a **known historical failure** is now green at **`3afa03f5`**. Remaining open items guard no known regression: §2 baseline rows, itemised REPL rows, and the new pyreadline stub-vs-real assertions. GCC pyreadline sign-off still rests on 2026-09-01.
5. **Docs:** **`65b7326a`** added [`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) — consolidated runnable MIN/FULL/REPL procedure for both toolchains, linked from both build guides, runtime notes §11 and the MIN doc.
6. **Lab note:** [`Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md`](./Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md).
7. **Tag cut:** **`python312-unified-full-lab-2026-09-04`** @ **`779b20cc`** — first pin containing the PyMod consolidation; supersedes **`python312-unified-full-lab-2026-09-01`** for clone-and-build (**§ Git tags**).

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
| V4.7 | GCC regression after INF edits | **Done** (2026-09-08 — clean-tree WSL **`BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE`** at **`9db93ae1`** + full hardware sweep; **`verify_pyconfig_gcc.sh`** not run, **`grep PLATFORM Include/pyconfig.h`** used instead) |

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
