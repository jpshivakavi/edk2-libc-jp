# Python 3.12.13 AppPkg — VS2022 / Windows Port Plan

## Related documents

| Document | Role |
|----------|------|
| [`Python312_AppPkg_Migration_Plan.md`](./Python312_AppPkg_Migration_Plan.md) | GCC-first AppPkg migration (Phases 0–8) |
| [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md) | GCC migration checklist and work log |
| [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md) | VS2022 phase checklist and session results |
| [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) | Windows / VS2022 build BKMs (Phase V1+) |
| [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md) | MIN vs FULL INF, VS2022 MIN commands |
| [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) | UEFI hang, 368 entry, deepfreeze, deploy, **GCC vs VS2022 REPL (§10.1)** |
| [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md) | GCC build walkthrough |
| [`Python-3.6.8/Python368.inf`](./Python-3.6.8/Python368.inf) | MSVC INF patterns (reference only) |
| [`.github/workflows/build-python-uefi-vs2022.yaml`](../../.github/workflows/build-python-uefi-vs2022.yaml) | Python 3.6.8 VS2022 CI reference |
| [`Python368_Windows_VS2022_Build_Guide.md`](./Python368_Windows_VS2022_Build_Guide.md) | **BUILD_PYTHON368** on Windows (full walkthrough) |

---

## 1. Goal and starting point

### Goal

From a **Windows host**, produce `Python312.efi` with:

```text
build -t VS2022 -a X64 -b RELEASE ^
  -p <edk2-libc>\AppPkg\AppPkg.dsc ^
  -D BUILD_PYTHON312
```

using **`PACKAGES_PATH=<edk2>;<edk2-libc>`** only (same upstream story as GCC FULL).

### Starting point (GCC AppPkg migration)

The GCC AppPkg port is the functional baseline:

- Monolithic `Python312.inf`, `PyMod-3.12.13/`, Phase **8.1–8.5** vendored libs.
- **`[BuildOptions]`** today is **GCC-only** — no `MSFT` / `UEFI_MSVC_*` in `Python312.inf`.
- The AppPkg migration plan defers VS2022 to a follow-on (Phase 8 optional: **VS2022 / MSFT**).

### Reference for MSVC mechanics (not module parity)

| Reference | Use it for |
|-----------|------------|
| `Python-3.6.8/Python368.inf` | `MSFT:*_*_*_CC_FLAGS`, `/GL- /Oi-`, `/wd…`, `UEFI_MSVC_64`, toolchain-split `_ctypes` / `cpu*.nasm` / `libffi_msvc` |
| `.github/workflows/build-python-uefi-vs2022.yaml` | Windows CI: NASM, `edksetup.bat`, `srcprep.py`, `create_python_pkg.bat` |
| `PyMod-3.6.8/Include/pyconfig.h` | `UEFI_MSVC_64` sizing for `size_t`, `void*`, `off_t` |
| `PyMod-3.6.8/Modules/config.c` | `#if defined(UEFI_MSVC_32\|\|UEFI_MSVC_64)` around `_ctypes` (3.12 differs: ctypes enabled for GCC too) |
| Current `Python312.inf` + Phase 8 docs | **What** to build; GCC is **proof** the source set is coherent |

**Important:** Python 3.6.8 **GCC** did not ship `_ctypes`; 3.12 **FULL GCC** does. VS2022 must implement **ctypes in parallel to GCC**, not by copying 3.6.8’s “MSFT-only ctypes” product story — only the **build mechanism** (`libffi_msvc`) is copied from 3.6.8.

### Non-goals

- **IA32 VS2022** unless explicitly desired (see `build-python-uefi-ia32-vs.yaml` for 3.6.8).
- Desktop **`PCbuild/*.vcxproj`** — not the UEFI path.
- Reintroducing intel-sandbox packages on `PACKAGES_PATH`.

---

## 2. GCC vs VS2022 — structural deltas for 3.12

| Area | GCC FULL (today) | VS2022 work |
|------|------------------|-------------|
| `[BuildOptions]` | `GCC:*_*_*_CC_FLAGS` + `GCC:*_*_X64_PP_FLAGS` for libffi `.S` | Add full **`MSFT`** block: includes (`\` paths), defines, warning disables; arch **`/DUEFI_MSVC_64`** (X64) |
| libffi / `_ctypes` | `unix64.S`, `win64.S`, no `malloc_closure.c` | **MSFT path:** `libffi_msvc` (`win64.asm`, `ffi.c`, `prep_cif.c`, …); **`malloc_closure.c`** on MSFT (3.6.8 pattern). Tag `.S` **`\| GCC`**, MASM/asm **`\| MSFT`** |
| CPU timing asm | Not in 3.12 tree (GCC FULL builds without `cpu*.s`) | If link errors reference `_Py_rdtsc` / similar, port **`PyMod-3.6.8/Modules/cpu.nasm`** (adapt for 3.12) **`\| MSFT`** only |
| NASM | `edk2stack.nasm`, `edk2handler.nasm`, `rand_rdrand.nasm` | Same files; **`NASM_PREFIX`** required on Windows (CI uses Chocolatey NASM) |
| OpenSSL / zlib | Pure C in INF (+ one NASM for RDRAND) | Should compile under MSVC with same sources; expect **more `/wd…`** and possible **`/bigobj`** for huge translation units |
| `pyconfig.h` | Sized for LP64-style UEFI/GCC (e.g. `SIZEOF_LONG 8`) | Add **`UEFI_MSVC_64` / `UEFI_MSVC_32`** branches like 3.6.8; keep **`_MSC_VER`** branch for `SIZEOF_LONG` |
| `config.c` | `_ctypes` always in inittab | Keep for GCC; ensure MSFT-only sources match inittab (no init without objects) |
| Frozen / deepfreeze | Host Python on Linux/WSL | Host **Python 3.10+ on Windows** (same `frozen/` flow) |
| Packaging | `create_python_pkg.sh` | **`create_python_pkg.bat VS2022 RELEASE X64 …`** (already parameterized for VS2022) |

---

## 3. Phase map (mirrors AppPkg migration)

Use the same **MIN → link → package → smoke → FULL batches** rhythm as GCC, but gate each milestone on **`-t VS2022`** on **Windows**.

### Phase V0 — Prerequisites and baseline capture

**Duration estimate:** short  
**Exit criteria:** Documented diff baseline; 3.6.8 VS2022 still builds (sanity); 3.12 GCC FULL artifact path recorded for comparison.

#### Steps

1. On Windows 2022 + VS2022 + NASM: reproduce **3.6.8** CI flow locally (clone tianocore/edk2, `edksetup.bat`, `srcprep.py`, `build … BUILD_PYTHON368`, `create_python_pkg.bat`).
2. Capture **3.12 GCC FULL** `Python312.efi` size and smoke checklist from [`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md) (zlib, ctypes, hashlib, ssl).
3. Inventory **toolchain-conditioned** lines to add to `Python312.inf`:
   - All libffi `.S` entries → `| GCC`
   - New MSFT libffi_msvc + optional `cpu.nasm` → `| MSFT`
   - Copy MSFT `[BuildOptions]` skeleton from `Python368.inf`, extend `-I` for `Include/internal`, HACL, zlib, openssl, libffi, `PyMod-3.12.13/efi/Include`.
4. Search `PyMod-3.12.13` for **`UEFI_C_SOURCE` vs `_MSC_VER`** gaps (e.g. `Programs/python.c` / `main.c`, `posixmodule.c`, OpenSSL `e_os.h` under UEFI).
5. Branch strategy: e.g. `feature/python-3.12.13-vs2022` off current AppPkg branch.

#### Deliverable

- `Python312_VS2022_Migration_Status.md` (checklist + artifact paths)
- Optional one-page “INF diff vs Python368” table

---

### Phase V1 — Windows host and EDK workspace

**Exit criteria:** BaseTools built; `PACKAGES_PATH` and `EDK2_LIBC_PATH` documented for cmd.exe.

#### Steps

1. **Tools:** VS2022 Build Tools, NASM (≥ 2.15 recommended, same as GCC BKMs), Git, Python 3.10+ for `srcprep.py` / frozen.
2. **Repos (sibling clones under one parent, e.g. `c:\Users\njayapra\github\`):**
   - **`WORKSPACE`** = tianocore **`edk2`** (e.g. `c:\Users\njayapra\github\edk2`) — `edksetup.bat`, `Build\`.
   - **`EDK2_LIBC_PATH`** = **edk2-libc fork** (active VS2022 workspace: e.g. `c:\Users\njayapra\github\edk2-libc-jp-vsfix` → remote **`jpshivakavi/edk2-libc-jp`**).
   - IDE workspace = that libc clone; EDK **`build`** still runs from **`edk2`**.
   - `set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix`
   - `set PACKAGES_PATH=c:\Users\njayapra\github\edk2;%EDK2_LIBC_PATH%`
3. **StdLib patches:** `git apply` the four patches under `Python-3.12.13/patches/` (same as GCC; not committed).
4. **`srcprep.py`** from `Python-3.12.13\`.
5. **Frozen:** On **`feature/python-3.12.13-vs2022`**, PyMod frozen artifacts are **in git** — no regen on fresh clone. Regenerate with **`Tools\build\regen_frozen_windows.cmd`** only when changing frozen inputs (WSL/Windows guides §6).
6. **`edksetup.bat`** in `edk2` WORKSPACE.

#### Deliverable

Short **Windows build BKMs** doc (parallel to `GCCCompilationBKMs.rst` / `Python312_WSL_GCC_Build_Guide.md`).

---

### Phase V2 — `pyconfig.h` and MSFT platform defines

**Exit criteria:** Preprocessor sees correct `SIZEOF_*` and `PLATFORM "uefi"` under `/DUEFI_MSVC_64`.

#### Steps

1. In `PyMod-3.12.13/Include/pyconfig.h`, port **`UEFI_MSVC_64` / `UEFI_MSVC_32`** blocks from 3.6.8 for at least: `SIZEOF_OFF_T`, `SIZEOF_SIZE_T`, `SIZEOF_VOID_P`, `SIZEOF_UINTPTR_T`, and **`SIZEOF_LONG`** via `#if defined(_MSC_VER)` (MSVC LLP64: `long` is 4 on X64).
2. Reconcile with existing 3.12 GCC-oriented defines (avoid breaking GCC when editing).
3. Run `srcprep.py` after header changes.
4. Optional: grep edk2-py312 / edk2-cpython `Include/pyconfig.h` for any **`UEFI_MSVC_*`** already used in the 3.12 upstream port.

---

### Phase V3 — `Python312.inf` MSFT `[BuildOptions]` (core compile flags)

**Exit criteria:** First **`build -t VS2022`** gets past compiling early C files (even if link fails later).

#### Steps

1. Add **`MSFT:*_*_*_CC_FLAGS`** modeled on `Python368.inf`:
   - `/GL- /Oi-` (required for this port class)
   - `/I$(EDK2_LIBC_PATH)\AppPkg\Applications\Python\Python-3.12.13\Include` and `\Include\internal`
   - PyMod efi Include, HACL, zlib, libffi includes, OpenSSL `-I` set (mirror GCC list with backslashes)
   - `-D UEFI`, `/DUEFI_C_SOURCE`, `HAVE_MEMMOVE`, `Py_BUILD_CORE`, `USE_PYEXPAT_CAPI`, `XML_STATIC`, `XML_POOR_ENTROPY`, **`NO_MSABI_VA_FUNCS`** (libffi)
   - `/WX-` and a **starting `/wd…` set** from 3.6.8; extend as MSVC surfaces 3.12-only warnings
2. **`[BuildOptions.X64]`:** `MSFT:*_*_*_CC_FLAGS = /DUEFI_MSVC_64`
3. **`[BuildOptions.IA32]`** (optional): `/DUEFI_MSVC_32` if pursuing IA32 later
4. Do **not** remove GCC options; keep dual-toolchain INF.

This phase implements AppPkg migration Phase 4 note: *“Do not assume MSFT flags yet.”*

---

### Phase V4 — Toolchain-split sources (MIN link on VS2022)

**Exit criteria:** **`Python312.efi` links** for a **MIN** module set on VS2022 X64.

**Recommended:** **MIN first** (comment out Phase 8 sources + inittab like Iteration 1), then re-enable batches in Phase V8. Alternative: attempt FULL in one pass if you prefer fewer INF variants — MIN is faster to debug MSFT flags.

#### Steps

1. Tag libffi assembly for GCC only:
   - `unix64.S`, `win64.S` → `| GCC`
2. Fix any NASM sources to build under MSFT tool tag (usually unqualified or `| MSFT` per edk2-py312 reference).
3. Resolve **first wave** of MSVC errors in PyMod overlays (common: `posixmodule.c`, `socketmodule.c`, `timemodule.c`, `Programs`/`main`, missing `crtdbg.h` guards — copy 3.6.8 / edk2-cpython patterns).
4. Link with same `[LibraryClasses]` / `[Protocols]` as GCC (already in INF).
5. Record output path, e.g. `Build\AppPkg\RELEASE_VS2022\X64\…\Python312.efi`.

#### MIN smoke

- Boot REPL; `import os, sys, json`
- Confirm ssl/ctypes/zlib **absent** if MIN

---

### Phase V5 — DSC / libc / packaging (Windows)

**Exit criteria:** Staged **`fs0:\EFI`** tree from Windows without manual copy hacks.

#### Steps

1. Confirm `AppPkg.dsc` `BUILD_PYTHON312` gate (unchanged).
2. `create_python_pkg.bat VS2022 RELEASE X64 <OutFolder>` with `WORKSPACE` = edk2 root.
3. Validate layout: `EFI\bin\Python312.efi`, `EFI\lib\python3.12\`, empty `lib-dynload`, readline staging if FULL.

---

### Phase V6 — Runtime smoke matrix (VS2022 MIN → FULL)

**Exit criteria:** Same functional bar as GCC FULL on UEFI Shell or QEMU.

#### MIN checklist

```text
[ ] Python312.efi starts; banner shows 3.12.13 / uefi
[ ] import os, sys, json, asyncio (if in INF)
[ ] read .py from FAT
[ ] import ssl, ctypes, zlib → fail or absent (MIN)
```

#### FULL checklist (after V8)

| After batch | Smoke |
|-------------|--------|
| V8.1 zlib | `import zlib` |
| V8.2 readline | REPL line editing / `import readline` if exposed |
| V8.5 ctypes | `import ctypes`; `ctypes.c_int(42)` |
| V8.3 hashlib | `hashlib.sha256(b"x").hexdigest()` |
| V8.4 ssl | `import ssl` |

Document **OpenSSL 1.1.1f** vs desktop 3.12 (see `Py312ReadMe.txt`).

---

### Phase V7 — Documentation and CI

**Exit criteria:** Repeatable CI artifact; docs list VS2022 as supported peer to GCC.

#### Steps

1. Update **`Py312ReadMe.txt`**: Windows/VS2022 section (not “GCC only”).
2. Add **`VS2022CompilationBKMs.rst`** or **`Python312_Windows_VS2022_Build_Guide.md`** (mirror WSL guide: patches, frozen, build, package, troubleshooting).
3. **CI:** New workflow **`build-python312-uefi-vs2022.yaml`** (clone edk2, NASM, `Python-3.12.13\srcprep.py`, `build … BUILD_PYTHON312`, `create_python_pkg.bat`, upload artifact) — parallel to existing 3.6.8 workflow.
4. Optional: extend AppPkg migration **Phase 7.3** to **dual** GCC (Linux) + VS2022 (Windows) on tianocore/edk2 WORKSPACE.
5. **7.5 hygiene:** Keep UEFI deltas in PyMod only; avoid new stock-tree forks for MSFT-specific fixes where possible.

---

### Phase V8 — Vendored FULL on VS2022 (same batches as GCC 8.x)

Execute in the **same order** as the GCC fork (**8.1 → 8.2 → 8.5 → 8.3 → 8.4**), but each step ends with **`build -t VS2022`** + smoke.

| Step | VS2022-specific focus |
|------|------------------------|
| **V8.1 zlib** | C-only; mostly `/wd` tuning |
| **V8.2 readline** | Pure Python + packaging; no MSFT nuance |
| **V8.5 ctypes** | **Largest MSVC chunk:** vendor **`libffi_msvc`** under `PyMod-3.12.13/Modules/_ctypes/libffi_msvc/` (from CPython 3.12 + 3.6.8 UEFI deltas + edk2-py312 if available); INF lines from `Python368.inf` adapted to PyMod paths; **`win64.asm`** for X64; enable **`malloc_closure.c`** for MSFT; keep GCC on edk2-libffi vendored tree |
| **V8.3–V8.4 OpenSSL** | Long compile; watch **command-line length** / **`/bigobj`**; `rand_rdrand.nasm` must assemble; verify no Perl-generated asm in INF (AppPkg tree is C-only) |

**Reference for V8.5 file list:** edk2-py312 `LibFFI.inf` / `PythonExtLib.inf` **and** 3.6.8 `Python368.inf` MSFT `_ctypes` section — not stock PC `_ctypes.vcxproj` alone.

See also:

- [`Python312_Phase8_8.1_Zlib.md`](./Python312_Phase8_8.1_Zlib.md)
- [`Python312_Phase8_8.2_Readline.md`](./Python312_Phase8_8.2_Readline.md)
- [`Python312_Phase8_8.5_Ctypes.md`](./Python312_Phase8_8.5_Ctypes.md)
- [`Python312_Phase8_8.3_Hashlib.md`](./Python312_Phase8_8.3_Hashlib.md)
- [`Python312_Phase8_8.4_Ssl.md`](./Python312_Phase8_8.4_Ssl.md)

---

## 4. Execution strategy (risk ordering)

1. **MIN VS2022 link** before touching ctypes (isolates `/wd` and pyconfig issues).
2. **zlib + hashlib (C)** before **ctypes** if doing incremental FULL (validates OpenSSL/zlib MSVC flags without MASM libffi).
3. **ctypes** — use proven GCC order **8.5 before 8.3–8.4** on the fork.
4. Keep **GCC green** after each INF change (dual-toolchain regression).

---

## 5. Troubleshooting playbook (Windows-specific)

| Symptom | Likely cause | Action |
|---------|----------------|--------|
| NASM not found | `NASM_PREFIX` | e.g. `C:\NASM\` on this host (CI often `C:\Program Files\NASM\`) |
| `.S` / `unix64.S` errors under VS2022 | Missing `\| GCC` | Toolchain-tag libffi asm |
| Unresolved `ffi_*` / closure symbols on MSFT | Missing `libffi_msvc` / `win64.asm` | Phase V8.5 INF |
| Wrong `SIZEOF_*` / struct layout crashes | Missing `UEFI_MSVC_64` | Phase V2 pyconfig |
| C2378 / too many sections | Huge OpenSSL `.c` | `/bigobj` on MSFT for module or file |
| Frozen/deepfreeze mismatch | Host Python version / stale generated files | Regenerate on Windows; compare with GCC tree |
| Path too long | Deep OpenSSL tree | Shorter clone path; enable long paths |

---

## 6. Deliverables summary

| # | Deliverable |
|---|-------------|
| 1 | Dual-toolchain `Python312.inf` (GCC + MSFT `[BuildOptions]` + split asm) |
| 2 | Updated `PyMod-3.12.13/Include/pyconfig.h` with `UEFI_MSVC_*` |
| 3 | `libffi_msvc` tree + INF entries for MSFT `_ctypes` |
| 4 | Windows build guide + `Py312ReadMe.txt` VS2022 section |
| 5 | `build-python312-uefi-vs2022.yaml` artifact |
| 6 | `Python312_VS2022_Migration_Status.md` tracking V0–V8 with smoke results |
| 7 | Optional: IA32 VS2022 workflow (defer) |

---

## 7. Mapping to AppPkg GCC phases

| AppPkg GCC phase | VS2022 phase |
|------------------|--------------|
| 0 Baseline | **V0** |
| 1 Scaffold | **V1** (workspace; tree exists) |
| 2 PyMod | **V2** + overlay MSFT fixes in **V4** |
| 3 Frozen | **V1** (Windows host) |
| 4 INF | **V3–V4** (MSFT options + splits) |
| 5 DSC / patches | **V1 + V5** |
| 6 MIN smoke | **V4–V6** MIN |
| 7 Docs / CI | **V7** |
| 8 Vendored FULL | **V8.1–V8.4** (V8.5 = ctypes MSFT) |

---

## 8. Suggested immediate next actions

1. Run **Phase V0**: build 3.6.8 with VS2022 locally; diff `Python368.inf` `[BuildOptions]` + MSFT-only `[Sources]` against `Python312.inf`.
2. **Phase V2 + V3**: pyconfig `UEFI_MSVC_64` + MSFT flags with **MIN** INF (Phase 8 commented out).
3. First **`build -t VS2022 -D BUILD_PYTHON312`** on X64 NOOPT or RELEASE; iterate `/wd` and PyMod until link.
4. Re-enable Phase 8 batches one at a time, finishing **V8.5 libffi_msvc** before assuming ctypes parity.

---

## 9. Progress snapshot (through 2026-07-23)

**Branch:** `feature/python-3.12.13-vs2022` on **`jpshivakavi/edk2-libc-jp`** · **Active clone:** `edk2-libc-jp-vsfix`  
**Authoritative checklist:** [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md) (Sessions 1–10)

| Milestone | Status |
|-----------|--------|
| V1–V5 workspace, pyconfig, INF MSFT flags, FULL link, Windows packaging | **Done** |
| V6 **MIN** UEFI smoke (VS2022) | **Done** — REPL, **`exit(0)`**, Shell **`exit`**, stub **`import readline`** |
| V6 **FULL** UEFI smoke | **Open** |
| V8 vendored sources on VS2022 link | **Done** (Session 6) |
| V7 CI + Py312ReadMe VS2022 | **Partial** |

**Recent commits (runtime):**

- **`59000200`** — VS2022 REPL exit / Shell teardown (stdio default, edk2console detach, finalize skips, 368 entry)
- **`3814cf9a`** — UEFI **`readline.py`** stub unless **`PY_UEFI_READLINE=1`**; ConIn **`CloseProtocol`**

### VS2022 vs GCC — intentional runtime divergence

Same monolithic **`Python312.inf`** and same staged **`EFI/lib/python3.12/`** layout do **not** mean identical Shell behavior:

1. **Entry:** GCC uses **custom stack + IDT**; VS2022 MIN uses **`PY_UEFI_MSVC_368_ENTRY`** (3.6.8-style **`ShellCEntryLib`** only). See runtime notes **§4** and deviations **§11**.
2. **REPL:** GCC Phase 8 reference smoke used **pyreadline** + line editing; **VS2022 manufacturing** is signed off on **stdio REPL** with pyreadline **opt-in** only (Session 10). Shared source policy (`readline.py`, `site.py`, `main.c`) may change packaged **GCC** UX until WSL regression is recorded.
3. **Deploy:** After Session 10, refresh **`Python312.efi`** **and** on-disk **`readline.py`** / **`site.py`** (or full **`create_python_pkg.*`**) — not binary-only for Lib changes.

**Not on branch:** local experiment to turn **default pyreadline on** for VS2022 was reverted before push.

---

## Reference paths (local)

| Item | Path |
|------|------|
| 3.12 AppPkg tree | `AppPkg/Applications/Python/Python-3.12.13/` |
| Monolithic INF | `Python-3.12.13/Python312.inf` |
| UEFI overlays | `Python-3.12.13/PyMod-3.12.13/` |
| 3.6.8 MSVC reference | `AppPkg/Applications/Python/Python-3.6.8/` |
| edk2-py312 (extension reference) | `c:\Users\njayapra\github\edk2-py312` |
| This plan | `AppPkg/Applications/Python/Python312_VS2022_Port_Plan.md` |
| Windows WORKSPACE (example) | `c:\Users\njayapra\github\edk2` |
| Fork libc clone (example) | `c:\Users\njayapra\github\edk2-libc-jp-vsfix` → remote **`jpshivakavi/edk2-libc-jp`** |
