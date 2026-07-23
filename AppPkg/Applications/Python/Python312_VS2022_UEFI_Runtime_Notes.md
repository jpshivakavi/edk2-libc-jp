# Python 3.12.13 AppPkg — VS2022 UEFI runtime notes

Companion to [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md), [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md), and [`Python368_Windows_VS2022_Build_Guide.md`](./Python368_Windows_VS2022_Build_Guide.md).

Documents **VS2022-specific** firmware entry, link, frozen/global-string, and smoke-test findings (2026-07). **Git workspace:** **`edk2-libc-jp-vsfix`**, branch **`feature/python-3.12.13-vs2022`**, remote **`jpshivakavi/edk2-libc-jp`**.

---

## 1. MIN vs FULL (two INF files)

EDK **does not** allow `!if $(BUILD_PYTHON312_FULL)` inside **`Python312.inf`** `[Sources]` or `[BuildOptions]` (error 3000). Use **two modules** instead:

| Build | `AppPkg.dsc` selects | Define |
|--------|----------------------|--------|
| **MIN** (default) | `Python-3.12.13/Python312_MIN.inf` | `BUILD_PYTHON312` only; `DEFINE BUILD_PYTHON312_FULL = FALSE` |
| **FULL** | `Python-3.12.13/Python312.inf` | `-D BUILD_PYTHON312_FULL=TRUE` |

**MIN** omits Phase 8: vendored **zlib**, **ctypes/libffi**, **OpenSSL** (`_ssl` / `_hashopenssl`). **`config.c`** uses `#ifdef BUILD_PYTHON312_FULL` for extension inittab entries.

**FULL** still supplies **`__chkstk`** via **`libffi_msvc/ffi.c`**. **MIN** does not link ctypes; see §3.

After switching MIN ↔ FULL, clean the other module output once if the linker picks up stale objects:

```cmd
rd /s /q %WORKSPACE%\Build\AppPkg\NOOPT_VS2022\X64\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312
rd /s /q %WORKSPACE%\Build\AppPkg\NOOPT_VS2022\X64\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312_MIN
```

---

## 2. Comparison: working **3.6.8 VS2022** vs **3.12**

| Topic | **Python 3.6.8** (`Python368.inf`) | **Python 3.12** (AppPkg migration) |
|--------|-------------------------------------|-------------------------------------|
| **`ENTRY_POINT`** | **`ShellCEntryLib`** → StdLib **`ShellAppMain`** → **`main`** | **`UefiMain`** in **`edk2main.c`**, then **`ShellCEntryLib`** |
| **Stack** | Firmware / Shell default stack | **`malloc(64MiB)` + `edk2_switch_stack`** (GCC path; see §4) |
| **Exceptions** | No custom IDT in INF | **`py_install_idt()`** before libc (GCC path) |
| **EFI glue** | **`edk2module.c`** | **`PyMod-3.12.13/efi/src/*`**, **`edk2console`** |
| **Path setup** | **`getpath.c`** | **`getpath.py`** + frozen/deepfreeze |
| **`Py_DEBUG`** in UEFI **`pyconfig.h`** | **`#undef Py_DEBUG`** | **`#define Py_DEBUG 1`** (more asserts; see §6) |
| **`python.c`** | **`main` + `Py_Main`** (wchar argv) when not `MS_WINDOWS` | **`UEFI_C_SOURCE`**: **`main` + `Py_BytesMain`** |

3.6.8 on UEFI Shell is the reference for **“Shell entry without custom stack.”**

---

## 3. Link: **`__chkstk`** (MIN only)

MSVC emits **`__chkstk`** for large stack frames (e.g. **`libmpdec/transpose.c`**). **FULL** resolves it from **`Modules/_ctypes/libffi_msvc/ffi.c`** (same hack as 3.6.8).

**MIN** has no ctypes. Add:

- **`PyMod-3.12.13/efi/src/msvc_chkstk.c`** — empty stub under **`_MSC_VER` && `UEFI_C_SOURCE`**
- **`Python312_MIN.inf`**: `PyMod-3.12.13/efi/src/msvc_chkstk.c | MSFT`

Do **not** add **`msvc_chkstk.c`** to **`Python312.inf`** (duplicate symbol with **`ffi.c`**).

---

## 4. VS2022 runtime hang — root cause and workaround

### Symptom

**FULL** and **MIN** VS2022 builds **linked**, but on UEFI Shell the image **hung** with **no** banner, **no** **`-h`**, prompt never returned. **GCC FULL** and **VS2022 3.6.8** worked on the same hardware.

**MIN** proved Phase 8 (OpenSSL/ctypes/zlib) was **not** the cause.

### Boot trace (diagnosis)

Optional **`PY_UEFI_BOOT_TRACE`** (see §7) prints **`Python312 boot: …`** via firmware **`Print()`**:

| Last message seen | Meaning |
|-------------------|---------|
| **`UefiMain enter`** … **`before ShellCEntryLib`** | Hang **inside** **`ShellCEntryLib` / `ShellAppMain` / `main` / `Py_Initialize`** |
| **`ShellAppMain enter`** … **`before main()`** | StdLib **stdio** or **`ArgvConvert`** |
| **`main enter`** … **`before Py_BytesMain`** | **`_PyMem_SetupAllocators`** or early interpreter |
| **`after ShellCEntryLib`** | Normal return from **`main`** (e.g. **`-h`**) |

**`fputs` to stdout** in **`python.c`** often **does not** appear on the Shell until StdLib console is up; prefer **`Print()`** for early diagnosis.

### Cause (VS2022)

Hang occurred **after** **`edk2_switch_stack`** and **`py_install_idt`**, **inside** **`ShellCEntryLib`**, on the **switched** stack. **GCC** uses the same **`UefiMain`** path successfully; **MSVC** does not in that configuration.

### Workaround: **`PY_UEFI_MSVC_368_ENTRY`**

When **`PY_UEFI_MSVC_368_ENTRY`** is defined (**`Python312_MIN.inf`** MSFT **`CC_FLAGS`** today), **`UefiMain`** still sets **`g_edk2_globals`** and **`edk2_alloc_environ`**, then calls **`ShellCEntryLib`** on the **default Shell stack** — **no** switch, **no** custom IDT — matching **3.6.8** entry behavior while keeping **`edk2console`** globals.

```c
/* edk2main.c — after protocol setup and edk2_alloc_environ() */
#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_368_ENTRY)
  status = ShellCEntryLib(image, systab);
  ...
#endif
```

**Tradeoffs:** No **64 MiB** dedicated stack; no custom IDT during the run — deep recursion or some faults may differ from GCC. Re-enable the full switch/IDT path on MSVC after **`edk2_switch_stack`** / alignment is fixed for VS2022.

**Verified with workaround:** **`Python312.efi -h`** shows full help and returns to the Shell (**`after ShellCEntryLib`**).

Apply the same define to **`Python312.inf`** MSFT flags when testing **FULL** on VS2022 until a proper stack fix lands.

---

## 5. Frozen / global strings / **`deepfreeze.c`**

### Runtime assert: **`PyUnicode_GET_LENGTH(string) != 1`**

With **`Py_DEBUG`** enabled in UEFI **`pyconfig.h`**, **`_PyUnicode_InitStaticStrings`** uses C **`assert()`** to ensure **one-character** strings are **not** registered as **`_Py_ID`** (they must use **`_Py_LATIN1_CHR`**).

**Upstream 3.12.13** has **no** **`STRUCT_FOR_ID(_)`** and **no** single-letter **`STRUCT_FOR_ID(a)`** etc. in **`pycore_global_strings.h`**.

This tree had **`deepfreeze.c`** using **`&_Py_ID(_)`** and **`&_Py_ID(b)`** … after regenerating globals from a scan, which triggered asserts (e.g. on **`_`**, line 54 of **`pycore_unicodeobject_generated.h`**) when running:

```text
Python312.efi -S -c "import sys; print(sys.version)"
```

**`-h`** can succeed without hitting full unicode static init; **`-S -c`** does not.

### Fixes (maintain on branch)

1. **`Tools/build/generate_global_objects.py`**
   - **`IGNORED`**: includes **`'_'`** (documented).
   - **`get_identifiers_and_strings`**: skip **`len(name) == 1`** when adding **`_Py_ID`** identifiers (matches upstream: no 1-char global IDs).

2. **`Python/deepfreeze/deepfreeze.c`**
   - Replace **`&_Py_ID(x)`** where **`x`** is a **single character** with **`_Py_LATIN1_CHR('x')`** — **no** **`&`** (macro is not an l-value).
   - Example: **`_Py_LATIN1_CHR('_')`**, **`_Py_LATIN1_CHR('b')`**.

3. **Regenerate headers** after changing globals or deepfreeze ID usage:

   **Windows (Python 3.12.x host):**

   ```cmd
   cd /d %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13
   Tools\build\regen_frozen_windows.cmd
   ```

   **Globals only** (after `deepfreeze.c` is already fresh):

   ```cmd
   py -3.12 %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13\Tools\build\generate_global_objects.py
   ```

4. **After regenerating `deepfreeze.c` from WSL/GCC**, run on Windows if needed:

   ```cmd
   py -3 %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13\Tools\build\fix_deepfreeze_latin1.py
   py -3 %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13\Tools\build\generate_global_objects.py
   ```

### MSVC compile errors from wrong fix

| Error | Cause | Fix |
|--------|--------|-----|
| **C2102** **`&` requires l-value** | **`&_Py_LATIN1_CHR('…')`** | Use **`_Py_LATIN1_CHR('…')`** only |
| **C2039** **`_py_b` is not a member** | **`&_Py_ID(b)`** but **`b`** not in generated struct | Latin1 + regenerate globals (§5) |

---

## 6. **`Py_DEBUG`** on UEFI

| Version | UEFI **`pyconfig.h`** |
|---------|------------------------|
| **3.6.8** | **`#undef Py_DEBUG`** |
| **3.12** | **`#define Py_DEBUG 1`** |

Effects: extra interpreter checks; C **`assert()`** in generated unicode init (§5). For a **release-like** UEFI image, consider **`#undef Py_DEBUG`** in **`PyMod-3.12.13/Include/pyconfig.h`** (and **`efi/Include`**, then **`srcprep.py`**) after runtime is stable — optional, separate from the **`_Py_ID` / deepfreeze** fix.

---

## 7. Debug defines (temporary)

Controlled from **`AppPkg.dsc`** when **`BUILD_PYTHON312`**:

| Define | Purpose |
|--------|---------|
| **`PY_UEFI_BOOT_TRACE=1`** | **`Print()`** in **`edk2main.c`**, **`Main.c`**, **`python.c`** (`py312_boot_print_ascii`), unhandled fault line in **`edk2excep.c`** |
| **`PY_UEFI_MSVC_368_ENTRY=1`** | MSVC: **`ShellCEntryLib`** without stack switch/IDT (§4) |

Also set on **`Python312_MIN.inf`** / **`Python312.inf`** MSFT **`CC_FLAGS`** as needed. Remove or gate behind a single **`PY_UEFI_DEBUG`** when V6 smoke is done.

**StdLib `Main.c`** boot lines require **`PY_UEFI_BOOT_TRACE`** on the **LibC** compile (via **`AppPkg.dsc`** `[BuildOptions]` when **`BUILD_PYTHON312`**).

---

## 8. Packaging and deployment

### Package

```cmd
set WORKSPACE=c:\Users\njayapra\github\edk2
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix
cd /d %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13
create_python_pkg.bat VS2022 NOOPT X64 <OutFolder>
```

**`create_python_pkg.bat`** locates **`Python312.efi`** under:

- `Build\AppPkg\…\Python312_MIN\DEBUG\` (MIN), or  
- `Build\AppPkg\…\Python312\DEBUG\` (FULL),

and stages **`EFI\bin`**, **`EFI\lib\python3.12\`**, **`EFI\stdlib\etc\`**. **`PREFIX`** is **`\\EFI`** on the volume where **`Python312.efi`** lives.

### Deploy

Copy **`<OutFolder>\EFI`** to the FAT volume root (e.g. **`fsN:\EFI`**). From Shell: **`map -r`**, **`fsN:`**, **`cd EFI\bin`**, **`Python312.efi`**.

### Binary-only swap

Replacing only **`EFI\bin\Python312.efi`** is OK for **C-only** interpreter changes if **`EFI\lib\python3.12`** is already correct. You **must** recopy **`EFI\lib\python3.12\`** (or run **`create_python_pkg.bat`**) after changes to **`Lib/site.py`**, **`readline.py`**, or other on-disk stdlib files — those are **not** embedded in the **`.efi`**.

---

## 10. Interactive REPL, pyreadline, and Shell **`exit`** (VS2022)

### Why this differs from **3.6.8**

| | **3.6.8 manufacturing** | **3.12 AppPkg (default)** |
|--|---------------------------|---------------------------|
| **`readline` on stick** | Not shipped; **`import readline`** fails silently | **`readline.py`** + **pyreadline** staged by **`create_python_pkg.bat`** |
| REPL input | StdLib **TTY** → **`fgets`** / **`PyOS_StdioReadline`** | Same **unless** pyreadline is enabled |
| **`edk2console`** | Not used | Used only when pyreadline installs **`PyOS_ReadlineFunctionPointer`** |

**3.6.8** never drives **`SimpleTextInputEx`** from Python for the REPL. **3.12** added Intel **pyreadline + `edk2console`** for line editing; on **VS2022** that path left **ConIn** / hooks in a state where **Shell `exit`** (return to firmware setup) **hung**, and a **second** **`Python312.efi`** launch could fail silently.

### Default policy (production — **2026-07-23**, user-verified)

**Pyreadline is off by default on UEFI.** The REPL behaves like **3.6.8**: basic **`>>>`** via StdLib console, **no** tab completion / pyreadline history.

| Layer | Behavior |
|--------|----------|
| **`Modules/main.c`** | **`pymain_import_readline`**: no-op under **`UEFI_C_SOURCE`** unless **`PY_UEFI_PYREADLINE`** at **compile** time |
| **`Lib/site.py`** | **`enablerlcompleter()`**: returns immediately when **`os.name == 'uefi'`** (no **`sys.__interactivehook__`** readline setup) |
| **`readline.py`** | On **`os.name == 'uefi'`**, **stub module** unless shell env **`PY_UEFI_READLINE=1`** (etc.) — **does not import pyreadline/edk2console** |
| **`Programs/python.c`** | Clears **`PyOS_ReadlineFunctionPointer`** and **`edk2_console_detach_readline()`** at **`main`** entry |
| **`Modules/main.c`** | **`edk2_console_detach_readline()`** before **`Py_FinalizeEx`** |
| **`edk2console.c`** | No periodic **1 ms** timer on module init; on detach: drain **ConIn**, **`CloseProtocol`** on **ConInEx**, clear readline hooks |
| **`edk2main.c`** | No post-**`ShellCEntryLib`** ConOut handoff (3.6.8 has none) |

**Verified flows (VS2022 MIN + 368 entry):**

```text
Python312.efi          → REPL → exit(0) → Shell → exit → firmware
Python312.efi -S       → REPL → exit(0) → Shell → exit → firmware
Python312.efi -S -I    → same (isolated; no site readline hook)
```

### Manual **`import readline`**

Before the **readline.py** stub fix, **`import readline`** still loaded **pyreadline → edk2console** even when hooks were “disabled,” which could **re-break Shell `exit`**.

**Now:** without **`PY_UEFI_READLINE=1`** in the environment, **`import readline`** is a **no-op stub** (no **`edk2console`**, no **`PyOS_ReadlineFunctionPointer`**). Safe for scripts that merely probe for the module.

### Optional: enable pyreadline (development only)

| Step | Action |
|------|--------|
| Compile | Add **`/DPY_UEFI_PYREADLINE=1`** to **`Python312_MIN.inf`** MSFT **`CC_FLAGS`** (opens **ConInEx** from **`UefiMain`**) |
| Runtime | Set **`PY_UEFI_READLINE=1`** (or **`yes`/`true`**) in the UEFI Shell environment before **`Python312.efi`** |
| Expect | Line editing may work; **Shell `exit`** after REPL is **not** signed off for manufacturing — teardown is improved (**detach**, **CloseProtocol**) but still under test |

### Do **not** (known bad on VS2022)

- **`ConIn->Reset(TRUE)`** or aggressive **ConOut->Reset** after REPL (blank screen / hang)
- Rely on **`.efi`-only** deploy after **`site.py`** / **`readline.py`** edits

### Bisect commands

| Test | Purpose |
|------|---------|
| **`Python312.efi -S -I`** | No site hook; stdio REPL only |
| **`Python312.efi -S`** | Site on, default UEFI readline off |
| **`import readline`** in REPL | Should stay stub unless **`PY_UEFI_READLINE=1`** |

---

## 9. Linker: **`/LTCG`**

EDK **NOOPT** VS2022 links often still pass **`/LTCG`**. Message *“/LTCG specified but no code generation required”* is a **performance hint** on incremental links, not a failure.

Disabling **`/LTCG`** on link (**`/LTCG:OFF`** via module **`MSFT:*_*_*_DLINK_FLAGS`** if supported) can change codegen; worth one A/B if chasing a MSVC-only runtime bug. It is unrelated to the **`_Py_ID` / deepfreeze** assert.

---

## 11. Recommended smoke order (VS2022 MIN + 368 entry)

```text
Python312.efi -h
Python312.efi -S -c "import sys; print(sys.version)"
Python312.efi -S -c "print(1+1)"
Python312.efi
Python312.efi -S
```

**MIN:** **`import ssl`** / **`import ctypes`** should fail. **`import hashlib`**, **`import os`** should work.

**User-verified (2026-07-23, VS2022 MIN + 368 entry):** **`-h`**, **`-S -c`**, default and **`-S`** REPL, **`exit(0)`** → Shell → **`exit`** → firmware (§10). **`import readline`** without **`PY_UEFI_READLINE=1`** must not break Shell **`exit`**.

Then enable **FULL** (`BUILD_PYTHON312_FULL=TRUE`), repackage, and repeat before Phase 8–specific tests (zlib, ssl, ctypes).

---

## 12. File index (VS2022 runtime touchpoints)

| Path | Role |
|------|------|
| [`AppPkg/AppPkg.dsc`](../../AppPkg.dsc) | MIN/FULL INF selection; **`PY_UEFI_BOOT_TRACE`** for LibC |
| [`Python-3.12.13/Python312_MIN.inf`](./Python-3.12.13/Python312_MIN.inf) | MIN sources, **`msvc_chkstk.c`**, MSFT flags |
| [`Python-3.12.13/Python312.inf`](./Python-3.12.13/Python312.inf) | FULL Phase 8 |
| [`PyMod-3.12.13/Modules/readline/readline.py`](./Python-3.12.13/PyMod-3.12.13/Modules/readline/readline.py) | UEFI stub unless **`PY_UEFI_READLINE`** |
| [`Lib/site.py`](./Python-3.12.13/Lib/site.py) | UEFI: skip **`enablerlcompleter`** |
| [`PyMod-3.12.13/efi/src/edk2console.c`](./Python-3.12.13/PyMod-3.12.13/efi/src/edk2console.c) | Detach readline; ConIn **`CloseProtocol`** |
| [`Modules/main.c`](./Python-3.12.13/Modules/main.c) | UEFI: detach readline before **`Py_FinalizeEx`** |
| [`PyMod-3.12.13/efi/src/edk2main.c`](./Python-3.12.13/PyMod-3.12.13/efi/src/edk2main.c) | **`UefiMain`**, 368-style MSVC path |
| [`PyMod-3.12.13/efi/src/msvc_chkstk.c`](./Python-3.12.13/PyMod-3.12.13/efi/src/msvc_chkstk.c) | MIN **`__chkstk`** |
| [`StdLib/LibC/Main/Main.c`](../../StdLib/LibC/Main/Main.c) | **`ShellAppMain`** boot **`Print`** |
| [`Tools/build/generate_global_objects.py`](./Python-3.12.13/Tools/build/generate_global_objects.py) | Global **`_Py_ID`** / unicode init headers |
| [`Tools/build/fix_deepfreeze_latin1.py`](./Python-3.12.13/Tools/build/fix_deepfreeze_latin1.py) | Normalize 1-char refs in **`deepfreeze.c`** |
| [`Tools/build/regen_frozen_windows.cmd`](./Python-3.12.13/Tools/build/regen_frozen_windows.cmd) | Windows frozen + deepfreeze + globals regen |
| [`Python-3.12.13/create_python_pkg.bat`](./Python-3.12.13/create_python_pkg.bat) | Windows packaging |

---

## 13. Related docs

- [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md) — build commands for MIN/FULL  
- [`Python312_UEFI_Startup_Messages.md`](./Python312_UEFI_Startup_Messages.md) — intentional console output (no debug **`Print`**)  
- [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md) — phase checklist (update **V6** when smoke is signed off)
