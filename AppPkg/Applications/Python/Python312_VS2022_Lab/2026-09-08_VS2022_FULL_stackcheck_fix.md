# Lab: VS2022 FULL — `PyOS_CheckStack` fix validated on hardware (2026-09-08)

**Branch:** `feature/python-3.12.13-vs2022` · **Code state:** **`c3819602`**
(*fix(python312): give PyOS_CheckStack a real bound on the MSVC 368 entry path* — the first
code-bearing commit after **`0a674ac0`**)
**Toolchain:** **VS2022 FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY`
**Result:** **PASS.** The deep-import case that used to hang Shell **`exit`** now raises a clean
`MemoryError` and **`exit` reaches BIOS setup with no hang.**

**Root-cause analysis this fix comes from:**
[`2026-09-07_VS2022_FULL_pyreadline_hang.md`](./2026-09-07_VS2022_FULL_pyreadline_hang.md).

---

## What was wrong

On the `PY_UEFI_MSVC_368_ENTRY` path, `edk2main.c` returns **before** the stack switch, so
`g_edk2_globals.stack` was never assigned. `PyOS_CheckStack()` compared `rsp` against that NULL
pointer, `rsp > 0` was always true, and the guard **always answered "stack is fine"** — despite
`USE_STACKCHECK 1` being set and the function being called from `Objects/object.c`,
`Python/ceval.c:260` and `Python/pythonrun.c:1913`. Deep recursion therefore ran off the ~128 KB
firmware stack silently and corrupted memory the Shell and BDS still needed, which is why
Python's own teardown always looked flawless and only Shell **`exit`** hung.

## Observed — the bound is live

First trace line of every run, printed under the already-enabled `PY_UEFI_BOOT_TRACE=1`,
immediately after `UefiMain enter` and before `ShellCEntryLib 368-style`:

```text
Python312: UefiMain
Python312 boot: UefiMain enter
Python312 boot: firmware stack rsp=6A969618 limit=6A951618 budget=18000
Python312 boot: ShellCEntryLib 368-style (no custom stack/IDT)
```

| Field | Value | Check |
|-------|-------|-------|
| `rsp` at `UefiMain` entry | **`0x6A969618`** | firmware stack top is just above this |
| `limit` | **`0x6A951618`** | `rsp − budget` exactly — arithmetic confirmed |
| `budget` | **`0x18000`** | 98 304 = **96 KB**, the `PY_UEFI_FIRMWARE_STACK_BUDGET` default |

A non-zero `limit` is the proof the fix is compiled in and active; it was effectively `0` before.

## Observed — the failure is now clean

```text
FS1:\EFI\bin\> Python312.efi -S -c "import json; print('ok')"
```

```text
Traceback (most recent call last):
  File "<string>", line 1, in <module>
  File "FS1:\EFI\lib\python3.12\json\__init__.py", line 106, in <module>
    from .decoder import JSONDecoder, JSONDecodeError
  File "FS1:\EFI\lib\python3.12\json\decoder.py", line 3, in <module>
    import re
  File "FS1:\EFI\lib\python3.12\re\__init__.py", line 124, in <module>
    import enum
  File "FS1:\EFI\lib\python3.12\enum.py", line 5, in <module>
    from functools import reduce
  File "FS1:\EFI\lib\python3.12\functools.py", line 18, in <module>
    from collections import namedtuple
  File "<frozen importlib._bootstrap>", line 1354, in _find_and_load
  ... _find_and_load_unlocked / _load_unlocked / module_from_spec /
      _init_module_attrs / cached / _get_cached ...
  File "<frozen importlib._bootstrap_external>", line 524, in cache_from_source
MemoryError: stack overflow
```

Teardown then ran to completion (`Py_RunMain after pymain_run_python`,
`edk2_console_detach_readline`, `Py_FinalizeEx …`) and **Shell `exit` reached BIOS setup with no
hang** — the behaviour this whole investigation was chasing.

### Which guard site fired

The message is **lowercase `stack overflow`**, which comes from **`Objects/object.c`**
(lines 418/548/601, the `PyObject_Repr` / `PyObject_Str` `USE_STACKCHECK` guards) — **not** the
capitalised `"Stack overflow"` at `Python/ceval.c:263` in `_Py_CheckRecursiveCall()`. Both sites
exist and either can fire; the `repr`/`str` path simply got there first, inside
`cache_from_source`.

### Depth reached, and why the budget looks well calibrated

| Command | Import nesting | Before the fix | After |
|---------|----------------|----------------|-------|
| `import re` | 4 — `re` → `enum` → `functools` → `collections` | clean | clean |
| `import json` | **6** — `json` → `json.decoder` → `re` → `enum` → `functools` → `collections` | **hung Shell `exit`** | **`MemoryError`, `exit` clean** |

The 96 KB budget therefore trips somewhere between **4 and 6 nested imports**, which is the same
band where the real firmware stack was already being breached. That is a good sign: the budget is
approximately calibrated to the hardware rather than arbitrarily tight or loose.

## Scope — what this does and does not fix

**Fixed:** the failure mode. Silent firmware-memory corruption became a catchable Python
exception, and Shell `exit` is reliable again.

**Not fixed:** the capability. `json`, `logging` and pyreadline are still unusable on VS2022,
because the underlying deviation is untouched — VS2022 still runs the interpreter on the firmware
stack while GCC gets 64 MB (`PY_UEFI_DEFAULT_STACK_SIZE`). Regaining depth needs the stack switch
working under MSVC; the cheapest experiment is to switch the stack but **skip `py_install_idt()`**,
since `edk2main.c:228` and `:231` are independent calls that the existing comments only ever
blame as a pair.

**Expected new signature:** pyreadline phases 2 and 3 should now raise `MemoryError` rather than
hang, because `pyreadline/logger.py` pulls `logging`. Worth confirming.

## Open — regression sweep not yet run

Phase 8 has **not** been re-run on `c3819602`. It should pass, because this port's Phase 8
imports are shallow (the staged `Lib/ssl/__init__.py` imports only `os`; `ctypes`, `hashlib` and
`zlib` are C extensions with little pure-Python nesting), but a budget that is too tight would
show up here first and it must be verified, not assumed.

## Tuning reference

`PY_UEFI_FIRMWARE_STACK_BUDGET` (96 KB) and `PY_UEFI_STACK_MARGIN` (8 KB) live in
`PyMod-3.12.13/efi/Include/efi/edk2stack.h`, both `#ifndef`-guarded so they can be overridden
from `CC_FLAGS`.

| Symptom | Meaning | Action |
|---------|---------|--------|
| Shell `exit` hangs again | Budget too large — overflow beats the check | `/DPY_UEFI_FIRMWARE_STACK_BUDGET=49152` |
| `MemoryError` on scripts that should fit | Budget too small | Raise it |
| Deep imports raise `MemoryError`, `exit` clean | **Current, working state** | Pursue the stack switch to regain depth |
