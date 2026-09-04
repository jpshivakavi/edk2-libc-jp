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

Apply the same defines to **`Python312.inf`** MSFT flags when testing **FULL** on VS2022 (mirrors **`Python312_MIN.inf`**).

---

## 5. Frozen / global strings / **`deepfreeze.c`**

### Why two steps (deepfreeze + globals)

**`PyMod-3.12.13/Tools/build/deepfreeze.py`** emits **`PyMod-3.12.13/Python/deepfreeze/deepfreeze.c`** with **`&_Py_ID(x)`** for **single-character** identifiers (e.g. **`d`**, **`_`**, **`s`**). This AppPkg fork’s **`PyMod-3.12.13/Tools/build/generate_global_objects.py`** deliberately **does not** register 1-character names as **`_Py_ID`** in **`pycore_global_strings.h`** (matches upstream 3.12 + avoids **`Py_DEBUG`** assert failures). Until upstream deepfreeze changes, you **must** run **`PyMod-3.12.13/Tools/build/fix_deepfreeze_latin1.py`** after any step that refreshes **`deepfreeze.c`** or **`generate_global_objects.py`** output.

**Symptoms if post-deepfreeze fixes are skipped:**

| Stage | Failure |
|--------|---------|
| **Runtime** (`Py_DEBUG`) | **`assert(PyUnicode_GET_LENGTH(string) != 1)`** in unicode static init (e.g. **`_Py_ID(_)`**) on **`-S -c`** — missing **latin1** fix |
| **Runtime** (`Py_DEBUG`) | **`unicodeobject.c`**: **`_PyUnicodeCheckConsistency`**: **`!_Py_IsImmortal(op)`** — immortal **`deepfreeze`** string without **`.statically_allocated = 1`** |
| **VS2022 compile** | **C2039**: **`_py_d`**, **`_py__`**, … **is not a member** of **`<unnamed-tag>`** in **`pycore_global_strings.h`** — missing **latin1** fix |

### Canonical regen order (Windows — use this)

**Preferred:** one script runs the full pipeline in order:

```cmd
cd /d %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13
Tools\build\regen_frozen_windows.cmd
```

**Host:** Python **3.12.x** (same major.minor as source; script asserts **`sys.version_info[:2] == (3, 12)`**). Optional: **`set HOSTPY=C:\Path\To\python.exe`**.

**Steps inside `regen_frozen_windows.cmd` (do not reorder):**

| Step | Tool | Output |
|------|------|--------|
| 1 | **`Programs\_freeze_module.py`** (×24 modules) | **`PyMod-3.12.13/Python/frozen_modules/*.h`** |
| 2 | **`PyMod-3.12.13/Tools/build/deepfreeze.py`** | **`PyMod-3.12.13/Python/deepfreeze/deepfreeze.c`** (may contain **`&_Py_ID(d)`**, etc.) |
| 2b | **`PyMod-3.12.13/Tools/build/fix_deepfreeze_statically_allocated.py`** | Ensures **`.statically_allocated = 1`** on immortal unicode **`.state`** (Py_DEBUG consistency) |
| 3 | **`PyMod-3.12.13/Tools/build/generate_global_objects.py`** | **`Include/internal/pycore_global_strings.h`**, **`pycore_runtime_init_generated.h`**, **`pycore_unicodeobject_generated.h`**, fini header |
| 4 | **`PyMod-3.12.13/Tools/build/fix_deepfreeze_latin1.py`** | Rewrites single-char **`&_Py_ID(c)`** → **`_Py_LATIN1_CHR('c')`** (no **`&`**) |
| 5 | **`findstr`** check | **`deepfreeze.c`** must contain **`.statically_allocated = 1,`** |

Then rebuild **`Python312.efi`** (MIN or FULL — same **`deepfreeze.c`**). Commit **`PyMod-3.12.13/Python/deepfreeze/deepfreeze.c`** and **`frozen_modules/*.h`** when they change.

### Fresh clone — build without regen

**Normal path:** clone/pull **`feature/python-3.12.13-vs2022`**, apply StdLib patches if needed, **`srcprep.py`**, **`build`**, **`create_python_pkg.bat`** / **`.sh`**. The repo ships **`PyMod-3.12.13/Python/frozen_modules/*.h`**, **`PyMod-3.12.13/Python/frozen.c`**, and **`PyMod-3.12.13/Python/deepfreeze/deepfreeze.c`** with **`.statically_allocated = 1`** and **`_Py_LATIN1_CHR`** for 1-char refs — you should **not** see **C2039** or **`_PyUnicodeCheckConsistency` / `!_Py_IsImmortal`** on **`Py_DEBUG`** UEFI builds **unless** you regenerate frozen artifacts incorrectly.

### Existing clone — after `git pull`

Re-run **`srcprep.py`** when overlay headers change; rebuild. No frozen regen unless you are editing frozen **`.py`** sources or running a deliberate refresh (below). Remove stale **`Python/frozen_modules/*.h`** if left over from an older layout (PyMod paths are canonical since **`55219522`**).

**You can still break runtime if you:**

| Mistake | Result |
|---------|--------|
| Run **`generate_global_objects.py`** alone | **C2039** on compile (latin1) |
| Run **`deepfreeze.py`** (or full regen) **without** **`fix_deepfreeze_statically_allocated.py`** and **`fix_deepfreeze_latin1.py`** | **C2039** and/or immortal unicode assert |
| Use an old commit’s **`deepfreeze.c`** before **`statically_allocated`** / latin1 fixes | Same asserts at runtime |

Always use **`Tools\build\regen_frozen_windows.cmd`** for a full refresh (includes both fix scripts after **`deepfreeze.py`**).

### Manual / partial regen (only if you know which artifact changed)

**Do not** run **`generate_global_objects.py`** alone on a tree whose **`deepfreeze.c`** still has single-char **`&_Py_ID`** — you will get **C2039** on the next MSVC build.

| Situation | Commands (from **`Python-3.12.13/`**) |
|-----------|----------------------------------------|
| **Full refresh** | **`Tools\build\regen_frozen_windows.cmd`** |
| **Only global headers** ( **`deepfreeze.c` already latin1-fixed** ) | **`py -3.12 PyMod-3.12.13\Tools\build\generate_global_objects.py`** — re-run **`fix_deepfreeze_latin1.py`** if **`deepfreeze.c`** was regenerated since last fix |
| **Only `deepfreeze.c`** (frozen `.h` already current) | **`py -3.12 PyMod-3.12.13\Tools\build\deepfreeze.py`** … **`-o PyMod-3.12.13/Python/deepfreeze/deepfreeze.c`** (same args as batch) → **`generate_global_objects.py`** → **`fix_deepfreeze_latin1.py`** |
| **WSL copy** from edk2-py312 | Copy into **`PyMod-3.12.13/Python/frozen_modules/`** and **`PyMod-3.12.13/Python/deepfreeze/`** — then **`python3 PyMod-3.12.13/Tools/build/generate_global_objects.py`**, **`fix_deepfreeze_statically_allocated.py`**, **`fix_deepfreeze_latin1.py`** |

**What `fix_deepfreeze_latin1.py` does:** regex replace **`&_Py_ID(.)`** → **`_Py_LATIN1_CHR('…')`**; strips erroneous **`&_Py_LATIN1_CHR`**. Prints **`remaining single-char &_Py_ID: 0`** when done.

### `generate_global_objects.py` fork rules (reference)

1. **`IGNORED`**: includes **`'_'`** (documented in script).
2. **`get_identifiers_and_strings`**: skip **`len(name) == 1`** when adding **`_Py_ID`** identifiers.

### Runtime assert (with **`Py_DEBUG`**)

With **`Py_DEBUG`** enabled in UEFI **`pyconfig.h`**, **`_PyUnicode_InitStaticStrings`** asserts that one-character strings are **not** registered as **`_Py_ID`**.

**`-h`** can succeed without full unicode static init; **`-S -c "import sys; print(sys.version)"`** does not if **`deepfreeze.c`** still references **`&_Py_ID(_)`** etc.

### MSVC / manual edit pitfalls

| Error | Cause | Fix |
|--------|--------|-----|
| **C2102** **`&` requires l-value** | **`&_Py_LATIN1_CHR('…')`** | Use **`_Py_LATIN1_CHR('…')`** only |
| **C2039** **`_py_x` is not a member** | **`&_Py_ID(x)`** for 1-char **`x`** after globals regen | **`py -3.12 PyMod-3.12.13\Tools\build\fix_deepfreeze_latin1.py`** (or full **`regen_frozen_windows.cmd`**) |
| **`_PyUnicodeCheckConsistency`** / **`!_Py_IsImmortal`** | Immortal **`deepfreeze`** unicode without **`.statically_allocated = 1`** | **`py -3.12 PyMod-3.12.13\Tools\build\fix_deepfreeze_statically_allocated.py`**, rebuild **`.efi`** |
| Stale `&_Py_STR(dot)` etc. | Old deepfreeze vs current **`deepfreeze.py`** | Full regen; some literals use **`_Py_SINGLETON(strings).ascii[N]`** (Session 6) |

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

### WIP: Shell **`exit`** hang — boot trace playbook (2026-08)

**Symptom:** **`Python312.efi -S -c "…"`** prints **`ok`**, returns to **`Shell>`**, then **`exit`** hangs (BIOS/setup never returns). **Shell `exit` produces no boot lines** — trace only covers **Python + `UefiMain`**.

**Build (WIP tree):** **`PY_UEFI_BOOT_TRACE=1`** is already on **`Python312_MIN.inf`** / **`Python312.inf`** MSFT **`CC_FLAGS`** and **`AppPkg.dsc`** MSFT LibC flags. Rebuild the module you test (MIN or FULL), deploy **`EFI\bin\Python312.efi`** (binary-only OK for these C traces).

**Run on firmware:** Scroll the console or capture serial/log. All lines look like **`Python312 boot: …`**.

**Expected ladder (368 entry, `-S -c` one-liner):**

| Order | Message | Layer |
|------:|---------|--------|
| 1 | **`UefiMain enter`** | Firmware entry |
| 2 | **`ShellCEntryLib 368-style …`** | Before Python |
| 3 | **`main enter`** … **`after Py_BytesMain`** | **`python.c`** |
| 4 | **`Py_RunMain after pymain_run_python`** | User **`-c`** finished |
| 5 | **`Py_FinalizeEx enter`** … **`leave`** | See sub-table below |
| 6 | **`Py_RunMain after Py_FinalizeEx`** | **`main.c`** |
| 7 | **`after ShellCEntryLib`** | Back in **`UefiMain`** |
| 8 | **`after edk2_free_environ`** | Back in **`UefiMain`** (no extra ConIn handoff here — Session 10 / 3.6.8 style) |
| 9 | **`before return from UefiMain`** | Python app done — **`Shell>`** should appear |

**`Py_FinalizeEx` sub-ladder (pinpoint hang inside finalize):**

| Last line seen | Suspect |
|----------------|---------|
| **`finalize_modules enter`** (never **`leave`**) | Module wipe / **`_PyModule_Clear`** / extension **`m_free`** |
| **`finalize_modules leave`**, hang before **`after finalize_modules`** | **`_PyEval_Fini`** / import fini |
| Hang after **`before call_ll_exitfuncs`** | C **`atexit`** / **`Py_AtExit`** callbacks (MIN) or **`fflush`** (FULL skips in WIP) |
| Full ladder through **`before return from UefiMain`**, then Shell **`exit`** hangs | **Post-Python** — Shell/firmware; compare MIN vs FULL and **`import ssl`** vs **`import sys`** |

**Record for each test:** build (**MIN/FULL**), command, **last boot line**, whether **`ok`** printed, whether **`Shell>`** returned, whether **`exit`** hung.

**Bisect commands (same stick, note last line each time):**

```text
Python312.efi -S -I -c "import sys; print('ok')"
Python312.efi -S -c "import sys; print('ok')"
Python312.efi -S -c "import ssl; print('ok')"
```

**Second launch:** If **`py312_uefi_reentry_cleanup enter`** appears at start of a **second** **`Python312.efi`** without reboot, the prior interpreter was still initialized — note that separately from Shell **`exit`** hangs.

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

### 10.1 GCC vs VS2022 (this port — not the same in practice)

| | **GCC FULL (reference smoke)** | **VS2022 MIN (manufacturing, 2026-07-23)** |
|--|--------------------------------|---------------------------------------------|
| **Firmware entry** | Custom stack + IDT, then **`ShellCEntryLib`** | **`PY_UEFI_MSVC_368_ENTRY`**: **`ShellCEntryLib`** on Shell stack only |
| **REPL input (signed off)** | Historically **pyreadline** + **Tab** ([`Python312_AppPkg_Migration_Status.md`](./Python312_AppPkg_Migration_Status.md) Phase 8.2) | **Stdio TTY** (`fgets` / **`PyOS_StdioReadline`**) — pyreadline **off** by default |
| **Same git policy (Session 10)** | **`readline.py` stub**, **`site.py`** skip, **`main.c`** skip apply when **`UEFI_C_SOURCE`** / **`os.name == 'uefi'`** — **GCC stick not re-smoked** after **`3814cf9a`** | **User-verified** REPL + Shell **`exit`** + safe stub **`import readline`** |
| **Docs** | [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) **§11** | This section |

**Build parity ≠ runtime parity.** VS2022 **requires** the 368 entry path and (for manufacturing) the stdio REPL / readline stub policy. Do not assume a green **GCC** pyreadline test implies **VS2022** can ship the same default without Shell teardown work.

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
| **`edk2main.c`** | No post-**`ShellCEntryLib`** ConOut handoff (3.6.8 has none; MIN verified) |
| **`pylifecycle.c`** | **FULL only:** skip shutdown **`PyGC_Collect`** / **`_PyGC_CollectNoFail`** on UEFI (**`BUILD_PYTHON312_FULL`**) — MIN keeps stock GC |
| **`Modules/_ssl.c`** | UEFI: no keylog lock; **`m_clear`/`m_free` no-op; **`moduleobject.c`** skips **`md_dict`** teardown for **`_ssl`** / **`ssl`** module objects |

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

## 10.5 FULL **`import ssl`** → Shell **`exit`** hang (VS2022, 2026-07)

| Test | Result |
|------|--------|
| **`Python312.efi -S -c "import sys; print(sys.version)"`** then Shell **`exit`** | OK |
| **`Python312.efi -S -c "import ssl; print('ok')"`** then Shell **`exit`** | **Hang** (Ctrl+Alt+Del) |
| **`import ctypes`** alone (no **`ssl`**) then Shell **`exit`** | OK (earlier stick) |

**Why `import ssl` hung but `import _ssl` did not:** **`import socket`** loads **`Lib/socket.py`**, which used to **`import selectors`** at module level (**`select` / `_select`**). That chain breaks **Shell `exit`** on VS2022 UEFI; **`import _socket`** alone does not load **`socket.py`**. **`socket.py`** now imports **`selectors`** only inside **`sendfile`** (lazy). Refresh **`EFI\\lib\\python3.12\\socket.py`** (and **`ssl.py`** if testing **`import ssl`**) on the stick — **`.efi`-only is not enough**.

**368 entry interaction:** With **`PY_UEFI_MSVC_368_ENTRY`**, Python returns from **`ShellCEntryLib`**, boot trace can reach **`before return from UefiMain`**, and the hang is still on **Shell `exit`** — firmware teardown after the app image returns, not a Python traceback.

**Do not call `edk2_console_handoff_to_shell()` after `ShellCEntryLib`:** An experiment added post-**`UefiMain`** handoff + **`py312_uefi_openssl_disarm`**; lab trace reached **`before return from UefiMain`** but **Shell `exit`** hung even for **`import sys`**. Console detach stays in **`Programs/python.c`** / **`Modules/main.c`** before **`Py_FinalizeEx`** (§10). **`UefiMain`** matches 3.6.8: **`edk2_free_environ`** then return only.

**Do not drop 368 on VS2022 FULL for ssl exit:** Removing **`/DPY_UEFI_MSVC_368_ENTRY=1`** from **`Python312.inf`** sends FULL through **malloc stack + `py_install_idt`**. On this hardware that **hangs inside `ShellCEntryLib`** (last boot line **`before ShellCEntryLib`**, never **`after ShellCEntryLib`**). **GCC FULL** uses that path successfully; **VS2022 FULL** still needs **368** to reach the interpreter. Fix Shell **`exit`** after **`import ssl`** with teardown/OpenSSL WIP, not by switching entry style.

**VS2022 FULL fix (2026-08 — single-run `import ssl` Shell `exit` hang):**

| Layer | Change |
|--------|--------|
| **`Lib/ssl.py`** (UEFI) | **`Purpose`** without **`_txt2obj`**; no **`Lib/socket.py`** at import; **`SSLContext = _SSLContext`**; **stub** **`SSLSocket`/`SSLObject`** (no full socket subtype / wiring at import) |
| **`pylifecycle.c` / `moduleobject.c`** | Skip **`_PyModule_Clear`** and leaky **`module_dealloc`** for **`ssl`** as well as **`_ssl`** / socket (avoid clearing pure **`ssl`** while **`_ssl`** teardown is MSVC-no-op) |
| **`py312_openssl_uefi.c`** | **`py312_uefi_phase8_after_finalize()`** → **`ERR_clear_error()`** after **`finalize_modules`** (MSVC FULL only) |
| **`_ssl.c` / `socketmodule.c`** | Keep MSVC UEFI **`m_clear`/`m_free`** no-ops and no **`WSACleanup`** atexit (documented) |

Redeploy **both** **`Python312.efi`** and **`EFI\lib\python3.12\ssl.py`**.

**FULL build policy (working tree):**

| Change | Role |
|--------|------|
| **`Python312.inf`**: keep **`/DPY_UEFI_MSVC_368_ENTRY=1`** | VS2022 FULL must boot (GCC-style entry hangs at **`ShellCEntryLib`**) |
| Teardown above | MSVC **`import ssl`** parity with GCC sign-off goal |
| **`OPENSSL_cleanup()`** no-op on **`OPENSSL_SYS_UEFI`** | Avoid stop handlers if anything calls cleanup after **`import ssl`** |

**Stick after rebuild + repackage:**

```text
Python312.efi -S -c "import ssl; print('ok')"
```

At **`Shell>`**, type **`exit`**. Must return to firmware without hang.

If boot hangs **before** banner with the GCC-style entry, re-add **`/DPY_UEFI_MSVC_368_ENTRY=1`** to **`Python312.inf`** (required for VS2022 FULL boot today).

---

## 11. Recommended smoke order (VS2022)

**Entry:** **MIN** and **FULL** use **`PY_UEFI_MSVC_368_ENTRY`** on VS2022. GCC-style stack + IDT is for **GCC FULL** only until MSVC entry parity is fixed.

### MIN (default DSC)

```text
Python312.efi -h
Python312.efi -S -c "import sys; print(sys.version)"
Python312.efi -S -c "print(1+1)"
Python312.efi -S -c "import os, sys, json; print('ok')"
Python312.efi
Python312.efi -S
Python312.efi -S -I
```

**MIN expectations:** **`import ssl`** / **`import ctypes`** should **fail**. **`import hashlib`**, **`import os`** should work. REPL → **`exit(0)`** → Shell **`exit`** → firmware; second **`Python312.efi`** shows banner. **`import readline`** without **`PY_UEFI_READLINE=1`**: stub only (§10).

**User-verified (2026-07-23, MIN):** manufacturing stdio REPL + teardown (§10).

### FULL (add after **`BUILD_PYTHON312_FULL=TRUE`**, repackage)

Repeat the MIN block, then:

```text
Python312.efi -S -c "import zlib; print(zlib.__name__)"
Python312.efi -S -c "import ctypes; print(ctypes.__name__)"
Python312.efi -S -c "import hashlib; print(hashlib.__name__)"
Python312.efi -S -c "import ssl; print(ssl.__name__)"
Python312.efi -S -c "import ssl; print('ok')"
```

After **each** one-liner, at **`Shell>`**, type **`exit`** (must reach firmware).

```text
Python312.efi -S -c "import zlib, ssl, ctypes, hashlib; print('phase8 ok')"
```

Confirm REPL teardown and relaunch again (no regression vs MIN).

**Authoritative checklist / sign-off table:** [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md) **Phase V6**.

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
| [`Tools/build/fix_deepfreeze_statically_allocated.py`](./Python-3.12.13/Tools/build/fix_deepfreeze_statically_allocated.py) | **`statically_allocated`** on deepfreeze unicode (Py_DEBUG) |
| [`Tools/build/regen_frozen_windows.cmd`](./Python-3.12.13/Tools/build/regen_frozen_windows.cmd) | Windows frozen + deepfreeze + globals regen |
| [`Python-3.12.13/create_python_pkg.bat`](./Python-3.12.13/create_python_pkg.bat) | Windows packaging |

---

## 13. Related docs

- [`Python312_VS2022_MIN_Build.md`](./Python312_VS2022_MIN_Build.md) — build commands for MIN/FULL  
- [`Python312_UEFI_Startup_Messages.md`](./Python312_UEFI_Startup_Messages.md) — intentional console output (no debug **`Print`**)  
- [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md) — phase checklist (update **V6** when smoke is signed off)
