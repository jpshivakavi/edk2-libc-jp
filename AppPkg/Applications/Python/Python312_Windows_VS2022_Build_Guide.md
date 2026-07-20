# Build Python 3.12.13 (AppPkg FULL) with VS2022 on Windows

Guide for the **edk2-libc AppPkg** tree on branch **`feature/python-3.12.13-vs2022`**.
This documents **Phase V1** host/workspace setup and will grow through **V7** as MSVC
support lands in `Python312.inf`.

**GCC reference (same tree):** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)

**Plan / status:**

- [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md)
- [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md)

**PACKAGES_PATH:** `<edk2>;<edk2-libc>` only (semicolon on Windows). Vendored zlib,
OpenSSL, libffi, and readline live under **`PyMod-3.12.13/Modules/`**.

---

## 0. Layout (Windows)

### Two directories (do not confuse them)

| What | Path | Role |
|------|------|------|
| **IDE / git “workspace”** | `c:\Users\njayapra\github\edk2-libc-jp` | Your **fork** of [tianocore/edk2-libc](https://github.com/tianocore/edk2-libc) — remote **`jpshivakavi/edk2-libc-jp`**. Python 3.12 AppPkg sources, `AppPkg.dsc`, patches, docs. **Edit and commit here.** |
| **EDK II `WORKSPACE`** | `c:\Users\njayapra\github\edk2` | **tianocore/edk2** clone. BaseTools, `Conf/`, `Build/` output, **`edksetup.bat`**. **Run `build` from here** after `edksetup`. |

The folder name **`edk2-libc-jp`** is only this machine’s clone name; EDK always sees it via
**`EDK2_LIBC_PATH`** (same as upstream docs’ `edk2-libc`).

**Sibling layout (locked for this developer setup):**

```text
c:\Users\njayapra\github\
  edk2\                 ← WORKSPACE (tianocore/edk2)
  edk2-libc-jp\         ← EDK2_LIBC_PATH (jpshivakavi fork of edk2-libc)
```

Do **not** set `PACKAGES_PATH=%CD%\edk2;%CD%` unless your current directory is
`c:\Users\njayapra\github` (parent of both clones). Prefer the explicit paths below.

**Environment (cmd.exe — set every new shell before `build`):**

```cmd
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp
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
cd c:\Users\njayapra\github\edk2-libc-jp
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
cd c:\Users\njayapra\github\edk2-libc-jp
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
   /mnt/c/Users/njayapra/github/edk2-libc-jp/AppPkg/Applications/Python/Python-3.12.13/Python/frozen_modules/
```

### Option B — edk2-py312 `make frozen` (Linux or WSL)

See [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md) §6.

### Option C — Windows-native freeze (future)

Requires host `_freeze_module` bootstrap (`edk2-py312` `make local_python` + `make frozen`,
or `make -f frozen/frozen_modules.mk` with `ROOT_DIR` pointing at a built host CPython).
Not validated in Phase V1.

### Verify

```cmd
cd AppPkg\Applications\Python\Python-3.12.13
if exist Python\deepfreeze\deepfreeze.c (echo deepfreeze OK) else (echo deepfreeze MISSING)
dir /b Python\frozen_modules\*.h | find /c /v ""
```

Expect **24** `.h` files (typical 3.12 set).

---

## 7. Build Python312 with VS2022 (Phase V3+ — not yet)

**Blocked until** `Python312.inf` gains MSFT `[BuildOptions]` and toolchain-split sources
(see port plan Phase V3–V4).

Placeholder command (do not expect success until MSVC port lands):

```cmd
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp
set PACKAGES_PATH=c:\Users\njayapra\github\edk2;%EDK2_LIBC_PATH%
cd c:\Users\njayapra\github\edk2
call edksetup.bat
build -t VS2022 -a X64 -b RELEASE -p %EDK2_LIBC_PATH%\AppPkg\AppPkg.dsc -D BUILD_PYTHON312
```

**Packaging** (after a successful build):

```cmd
set WORKSPACE=c:\Users\njayapra\github\edk2
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp
AppPkg\Applications\Python\Python-3.12.13\create_python_pkg.bat VS2022 RELEASE X64 myUEFIPy312
```

---

## 8. Troubleshooting (Phase V1)

| Issue | Action |
|-------|--------|
| `Cannot find BaseTools Bin Win32` | Run `pip-requirements.txt` + `Edk2ToolsBuild.py -t VS2022` |
| `No module named edk2toolext'` | `pip install -r edk2\pip-requirements.txt` |
| NASM not found | Set `NASM_PREFIX` (e.g. `C:\NASM\`) before `edksetup` / `build` |
| Missing `frozen_modules/*.h` | Copy from WSL or regenerate (§6) |
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
[ ] deepfreeze.c + 24 frozen_modules/*.h present
[ ] (Optional) BUILD_PYTHON368 smoke on VS2022
```

When all items are green, Phase V1 exit criteria are met — proceed to **Phase V2**
in [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md).
