# Build Python 3.12.13 (AppPkg FULL) with VS2022 on Windows

Guide for the **edk2-libc AppPkg** tree on branch **`feature/python-3.12.13-vs2022`**.
This documents **Phase V1** host/workspace setup and will grow through **V7** as MSVC
support lands in `Python312.inf`.

**GCC reference (same tree):** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)

**GCC vs VS2022 (INF splits, flags, libffi, OpenSSL glue, runtime divergence):** [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) — **§11** documents where VS2022 **deviates** from GCC for firmware entry and REPL.

**UEFI runtime (VS2022 hang, MIN, deepfreeze, deploy, GCC vs VS2022 REPL):** [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md)

**Plan / status:**

- [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md)
- [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md)
- [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md) — MIN vs FULL INF, VS2022 build
- [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) — hang, 368 entry, deepfreeze, packaging

**PACKAGES_PATH:** `<edk2>;<edk2-libc>` only (semicolon on Windows). Vendored zlib,
OpenSSL, libffi, and readline live under **`PyMod-3.12.13/Modules/`**.

**Active libc clone (this port):** **`edk2-libc-jp-vsfix`** — Cursor/git workspace and
**`EDK2_LIBC_PATH`** for VS2022 work on **`feature/python-3.12.13-vs2022`**. Git remote
is still **`jpshivakavi/edk2-libc-jp`**; the **`-vsfix`** suffix is only the local directory
name. EDK **`Build\`** paths include that folder name (e.g. `...\edk2-libc-jp-vsfix\...\Python312.efi`).

---

## 0. Layout (Windows)

### Two directories (do not confuse them)

| What | Path | Role |
|------|------|------|
| **IDE / git workspace** | `c:\Users\njayapra\github\edk2-libc-jp-vsfix` | **Fork** of [tianocore/edk2-libc](https://github.com/tianocore/edk2-libc) — **`jpshivakavi/edk2-libc-jp`**. Python 3.12 AppPkg, `AppPkg.dsc`, VS2022 fixes, docs. **Edit and commit here.** |
| **EDK II `WORKSPACE`** | `c:\Users\njayapra\github\edk2` | **tianocore/edk2** clone. BaseTools, `Conf/`, `Build/` output, **`edksetup.bat`**. **Run `build` from here** after `edksetup`. |

Generic docs say **`edk2-libc`**; on this machine set **`EDK2_LIBC_PATH`** to the **`edk2-libc-jp-vsfix`** clone.

**Sibling layout (locked for this developer setup):**

```text
c:\Users\njayapra\github\
  edk2\                    ← WORKSPACE (tianocore/edk2)
  edk2-libc-jp-vsfix\      ← EDK2_LIBC_PATH (VS2022 port workspace)
```

Do **not** set `PACKAGES_PATH=%CD%\edk2;%CD%` unless your current directory is
`c:\Users\njayapra\github` (parent of both clones). Prefer the explicit paths below.

**Environment (cmd.exe — set every new shell before `build`):**

```cmd
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix
set PACKAGES_PATH=c:\Users\njayapra\github\edk2;%EDK2_LIBC_PATH%
set NASM_PREFIX=C:\NASM\
cd /d c:\Users\njayapra\github\edk2
call edksetup.bat
```

Optional persistence:

```cmd
set WORKSPACE=c:\Users\njayapra\github\edk2
set EDK_TOOLS_PATH=%WORKSPACE%\BaseTools
set NASM_PREFIX=C:\NASM\
```

(`NASM` need not be on `PATH` if `NASM_PREFIX` ends with `\` and contains `nasm.exe`. **This machine:** `C:\NASM\nasm.exe` — NASM **3.02**.)

**Interim note:** GCC migration often used **`edk2-py312\edk2`** as WORKSPACE on WSL.
On Windows, use a **tianocore `edk2` clone** (same as the 3.6.8 VS2022 workflow) once
BaseTools build succeeds here.

---

## 1. Install host tools (Phase V1.1)

| Tool | Purpose | Check |
|------|---------|--------|
| **Git for Windows** | clone, `git apply` patches | `git --version` |
| **Python 3.10+** | `srcprep.py`, BaseTools Python | `python --version` |
| **Visual Studio 2022** with **Desktop C++** | `-t VS2022` EDK build | Developer Command Prompt or `edksetup.bat` loads VC |
| **NASM ≥ 2.15** | `edk2stack.nasm`, `edk2handler.nasm`, `rand_rdrand.nasm` | `nasm -v` on PATH or `NASM_PREFIX` |

**NASM (≥ 2.15):** EDK reads **`NASM_PREFIX`** (trailing backslash required).

**This developer setup:**

```cmd
set NASM_PREFIX=C:\NASM\
C:\NASM\nasm.exe -v
```

Alternative: [nasm.us](https://www.nasm.us/) or `choco install nasm` (often
`C:\Program Files\NASM\`). CI uses Chocolatey; local path may differ.

**VS2022:** Install *Desktop development with C++* workload. EDK `edksetup.bat` selects
an installed VS and extends `PATH` (see status log after first `edksetup`).

---

## 2. Clone and prepare edk2 (Phase V1.6)

```cmd
cd c:\Users\njayapra\github
git clone https://github.com/tianocore/edk2.git
cd edk2
git submodule update --init
```

**BaseTools (required before `build`):**

Modern edk2 uses Python to build native BaseTools binaries:

```cmd
cd c:\Users\njayapra\github\edk2
python -m pip install --upgrade pip
python -m pip install -r pip-requirements.txt
python BaseTools\Edk2ToolsBuild.py -t VS2022
call edksetup.bat
```

Verify:

```cmd
if exist BaseTools\Bin\Win32\build.exe (echo BaseTools OK) else (echo BaseTools MISSING)
```

If `Edk2ToolsBuild.py` fails with `No module named 'edk2toolext'`, install edk2
Python requirements from **`pip-requirements.txt`** in the edk2 root (see
[`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md) for
last run results).

**Optional smoke** (after BaseTools green): build **Python 3.6.8** with **`-D BUILD_PYTHON368`**
before attempting `BUILD_PYTHON312`. Full steps (env, srcprep, package, troubleshooting):

→ **[`Python368_Windows_VS2022_Build_Guide.md`](./Python368_Windows_VS2022_Build_Guide.md)**

---

## 3. edk2-libc branch (Phase V1.2)

```cmd
cd c:\Users\njayapra\github\edk2-libc-jp-vsfix
git checkout feature/python-3.12.13-vs2022
```

Confirm:

```cmd
dir AppPkg\Applications\Python\Python-3.12.13\Python312.inf
dir AppPkg\Applications\Python\Python-3.12.13\patches\*.patch
```

---

## 4. Apply libc patches (**required**, local only — Phase V1.3)

Same four patches as GCC. **Do not commit** `StdLib/` changes.

```cmd
cd c:\Users\njayapra\github\edk2-libc-jp-vsfix
git apply --check --ignore-whitespace AppPkg\Applications\Python\Python-3.12.13\patches\0001-Implement-minimal-emulation-of-pipe-functionality.patch
git apply --ignore-whitespace AppPkg\Applications\Python\Python-3.12.13\patches\0001-Implement-minimal-emulation-of-pipe-functionality.patch
git apply --ignore-whitespace AppPkg\Applications\Python\Python-3.12.13\patches\0002-Introduce-support-for-ANSI-escape-codes-for-console.patch
git apply --ignore-whitespace AppPkg\Applications\Python\Python-3.12.13\patches\0003-Fix-uninitialized-static-variable.patch
git apply --ignore-whitespace AppPkg\Applications\Python\Python-3.12.13\patches\0004-Fix-ioctl-vararg-handling-for-Console-and-Shell-devi.patch
dir StdLib\LibC\Uefi\upipe.c
```

Re-apply after resetting `StdLib` on a clean checkout.

---

## 5. srcprep (Phase V1.4)

```cmd
cd AppPkg\Applications\Python\Python-3.12.13
python srcprep.py
findstr PLATFORM Include\pyconfig.h
```

Expect `#define PLATFORM "uefi"`.

### Phase V2 proof (after any `pyconfig.h` edit)

From **VS2022 dev shell** or after **`edksetup.bat`**:

```cmd
cd AppPkg\Applications\Python\Python-3.12.13\vs2022_verify
verify_pyconfig_msft.bat
```

Expect: `OK: V2 MSVC pyconfig verify passed` (uses **`/DUEFI_MSVC_64`**, same as **`Python312.inf`** X64 MSFT flags).

On WSL, run **`vs2022_verify/verify_pyconfig_gcc.sh`** to confirm the GCC reference **`#else`** sizes.

---

## 6. Frozen / deepfreeze (Phase V1.5)

`Python312.inf` requires:

- `Python/deepfreeze/deepfreeze.c`
- `Python/frozen.c` includes headers under `Python/frozen_modules/*.h` (gitignored)

### Option A — Copy from WSL (fastest if GCC tree already built)

From WSL (adjust `~/src/edk2-libc` if needed):

```bash
cp ~/src/edk2-libc/AppPkg/Applications/Python/Python-3.12.13/Python/frozen_modules/*.h \
   /mnt/c/Users/njayapra/github/edk2-libc-jp-vsfix/AppPkg/Applications/Python/Python-3.12.13/Python/frozen_modules/
```

### Option B — edk2-py312 `make frozen` (Linux or WSL)

See [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md) §6.

Requires host `_freeze_module` bootstrap (`edk2-py312` `make local_python` + `make frozen`,
or `make -f frozen/frozen_modules.mk` with `ROOT_DIR` pointing at a built host CPython).

### Option C — Windows-native regen (Python 3.12.x host)

From **`Python-3.12.13`** (or anywhere; the script `cd`s to the tree root):

```cmd
cd /d c:\Users\njayapra\github\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13
Tools\build\regen_frozen_windows.cmd
```

**Host:** **Python 3.12.x** (e.g. **3.12.10** installer) — same **3.12** minor as this **3.12.13** source; marshal magic must be **168627659** (script checks `sys.version_info[:2] == (3, 12)`). Override interpreter: `set HOSTPY=C:\Path\To\python.exe` then run the batch file.

The script runs, **in order:** **`Programs\_freeze_module.py`** → **`deepfreeze.py`** → **`generate_global_objects.py`** → **`fix_deepfreeze_latin1.py`**, then verifies **`.statically_allocated = 1`** in **`deepfreeze.c`**.

**Important:** **`fix_deepfreeze_latin1.py`** is **required** after **`deepfreeze.py`** / **`generate_global_objects.py`**. Running **`generate_global_objects.py`** alone leaves single-char **`&_Py_ID`** in **`deepfreeze.c`** and breaks VS2022 with **C2039** (`_py_d`, `_py__`, …). See [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §5.

**Quick fix if the build already fails with C2039 on `deepfreeze.c`:**

```cmd
cd /d c:\Users\njayapra\github\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13
py -3.12 Tools\build\fix_deepfreeze_latin1.py
```

Expect **`remaining single-char &_Py_ID: 0`**, then rebuild.

### Verify

```cmd
cd AppPkg\Applications\Python\Python-3.12.13
if exist Python\deepfreeze\deepfreeze.c (echo deepfreeze OK) else (echo deepfreeze MISSING)
dir /b Python\frozen_modules\*.h | find /c /v ""
```

Expect **24** `.h` files (typical 3.12 set).

---

## 7. Build Python312 with VS2022 (Phase V3–V4)

```cmd
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix
set PACKAGES_PATH=c:\Users\njayapra\github\edk2;%EDK2_LIBC_PATH%
set NASM_PREFIX=C:\NASM\
cd /d c:\Users\njayapra\github\edk2
call edksetup.bat
build -t VS2022 -a X64 -b RELEASE -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312
```

Artifact (typical):

```text
edk2\Build\AppPkg\RELEASE_VS2022\X64\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312\DEBUG\Python312.efi
```

### Packaging (Phase V5)

Use **`create_python_pkg.bat`** from **`edk2-libc-jp-vsfix`**. The copy under plain **`edk2-libc`** is still a Phase 6 **stub** that only prints `TODO: Phase 6`.

```cmd
set WORKSPACE=c:\Users\njayapra\github\edk2
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix
cd /d %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13
create_python_pkg.bat VS2022 RELEASE X64 c:\Users\njayapra\github\edk2-libc-jp-vsfix\myUEFIPy312
```

On success you should see **`Python 3.12 EFI package ready at …\EFI\`** (not the TODO line). Output layout:

```text
myUEFIPy312\EFI\bin\Python312.efi
myUEFIPy312\EFI\lib\python3.12\
myUEFIPy312\EFI\stdlib\etc\
```

Copy the **`EFI\`** folder to the FAT volume root (e.g. `fs0:\EFI\`), then from UEFI Shell: `fs0:` → `cd EFI\bin` → `Python312.efi`.

### Deploy after Session 10 (REPL / readline)

| Change type | Redeploy |
|-------------|----------|
| C only (`edk2main`, `edk2console`, `main.c`, `pylifecycle.c`, …) | **`EFI\bin\Python312.efi`** |
| **`Lib/site.py`**, **`readline.py`**, other staged stdlib | Full **`EFI\lib\python3.12\`** or re-run **`create_python_pkg.bat`** |

**Manufacturing default (VS2022):** stdio REPL; **`import readline`** is a stub unless **`PY_UEFI_READLINE=1`**. **GCC** reference smoke historically used **pyreadline** — see [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) **§11**.

Recommended smoke (MIN): runtime notes **§11** (`-h`, `-S -c`, REPL → **`exit(0)`** → Shell **`exit`**).

---

## 8. Troubleshooting (Phase V1)

| Issue | Action |
|-------|--------|
| `Cannot find BaseTools Bin Win32` | Run `pip-requirements.txt` + `Edk2ToolsBuild.py -t VS2022` |
| `No module named edk2toolext'` | `pip install -r edk2\pip-requirements.txt` |
| NASM not found | Set `NASM_PREFIX` (e.g. `C:\NASM\`) before `edksetup` / `build` |
| Missing `frozen_modules/*.h` | Copy from WSL (§6 A), **`Tools\build\regen_frozen_windows.cmd`** (§6 C), or WSL `make frozen` (§6 B) |
| `git apply` fails | Apply patches one-by-one; use `--ignore-whitespace` |
| OpenSSL symlink on Windows | Optional for monolithic build; `git restore` path under `PyMod-.../LibOpenSSL/openssl` if needed |

---

## 9. Phase V1 checklist (quick)

```text
[ ] VS2022 C++ workload installed
[ ] NASM on PATH or NASM_PREFIX set
[ ] edk2 cloned; BaseTools built; edksetup.bat OK
[ ] EDK2_LIBC_PATH + PACKAGES_PATH set (cmd)
[ ] Four libc patches applied locally; upipe.c exists
[ ] srcprep.py run; PLATFORM "uefi"
[ ] deepfreeze.c + 24 frozen_modules/*.h present (or run Tools\build\regen_frozen_windows.cmd)
[ ] (Optional) BUILD_PYTHON368 smoke on VS2022
```

When all items are green, Phase V1 exit criteria are met — proceed to **Phase V2**
in [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md).
