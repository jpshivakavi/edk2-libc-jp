# Lab: VS2022 FULL — `PyOS_CheckStack` fix validated on hardware (2026-09-08)

**Branch:** `feature/python-3.12.13-vs2022` · **Code state:** **`c3819602`**
(*fix(python312): give PyOS_CheckStack a real bound on the MSVC 368 entry path* — the first
code-bearing commit after **`0a674ac0`**)
**Toolchain:** **VS2022 FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY`
**Result:** **PARTIAL, and correct as far as it goes.** The **non-interactive** deep-import case
now raises a clean `MemoryError` and Shell **`exit`** is clean. The **interactive REPL still
hangs**, but for an **unrelated second reason** — not the budget.

> **Superseded conclusion.** The *"Interactive REPL still hangs"* section below blames a too-large
> 96 KB budget. **Measurement disproved that.** The clean and hanging runs bottom out
> **256 bytes apart**, and `import re` clears `limit` by only **6 240 bytes** — the budget is
> nearly too *tight*, so lowering it would have broken working imports. The real cause is that
> REPL `exit()` goes through `Py_Exit()` and skips `edk2_console_detach_readline()`, leaving
> ConInEx open. See
> [`2026-09-08_VS2022_FULL_interactive_exit_leak.md`](./2026-09-08_VS2022_FULL_interactive_exit_leak.md).
> The reasoning about `PyOS_CheckStack()` being a *sampled* guard still stands and still argues for
> the stack switch; only the "budget is too generous" diagnosis was wrong.

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

## Interactive REPL still hangs — *~~the budget is too generous~~* (WRONG, see banner above)

Reported the same day, same image:

```text
FS1:\EFI\bin\> Python312.efi          (interactive)
>>> import json
    ... MemoryError displayed ...
>>> exit()                            -> returns to Shell fine
Shell> exit                           -> HANG
```

| Path | `MemoryError` raised? | Shell `exit` |
|------|----------------------|--------------|
| `-S -c "import json"` | yes | **clean** |
| interactive `>>> import json` | yes | **HANG** |

**Why the guard is not sufficient on its own.** `PyOS_CheckStack()` is a **sampled** check — it
only runs where CPython calls it (`Objects/object.c` `PyObject_Repr`/`Str`, and
`_Py_CheckRecursiveCall`). Between two sample points, arbitrarily much C stack can be consumed
without any check. So the distance from `stack_limit` down to the true firmware stack base has to
absorb the worst-case *unchecked* excursion, plus whatever the `MemoryError` unwind and traceback
formatting need.

The REPL starts deeper — `PyRun_InteractiveLoop` → `PyRun_InteractiveOne` → parser → eval frames
are all live when `import json` begins — and it formats and prints the traceback from inside that
deeper context. With a 96 KB budget out of an assumed ~128 KB stack, only ~32 KB of residual is
left for that, and the interactive path evidently exceeds it and reaches past the base. Note the
REPL *appears* to recover (`exit()` returns to the Shell) precisely because the corruption lands
in firmware memory Python never touches again — the same signature as the original bug.

**This strengthens the case for the real fix (the stack switch).** A sampled guard against an
*unknown* stack base is inherently fragile: any budget is a guess, and there is no value that is
provably safe. Getting VS2022 onto the 64 MB switched stack removes the guesswork entirely.

## Instrumentation added — size the budget from measurement, not guesswork

Rather than guess a smaller number, the next build reports how deep execution actually got.
Added to `edk2_globals_t`: `stack_entry_rsp` and `stack_min_rsp` (deepest rsp ever sampled by
`PyOS_CheckStack`), printed after `ShellCEntryLib` returns under the existing
`PY_UEFI_BOOT_TRACE=1`:

```text
Python312 boot: stack high-water min_rsp=... limit=... used=...
```

`used` is `stack_entry_rsp − stack_min_rsp`, i.e. total depth consumed.

**How this locates the stack base.** `min_rsp` is sampled, so it under-reports the true peak — but
it still **brackets** the base:

- a `min_rsp` from a run that **exits cleanly** is **above** the base
- a `min_rsp` from a run that **hangs** is at or **below** it

So capturing `min_rsp` from the clean `-c` run and from the hanging interactive run pins the base
between the two, and the budget can then be set with a real margin instead of an assumption.

**Collection procedure** — read the `stack high-water` line after each, before typing `exit`:

```text
Python312.efi -S -c "import re; print('ok')"       (clean baseline)
Python312.efi -S -c "import json; print('ok')"     (clean, guard fires)
Python312.efi                                       then: import json / exit()   (hangs at Shell exit)
```

## Scope — what this does and does not fix

**Fixed:** the failure mode **on the non-interactive path**. Silent firmware-memory corruption
became a catchable Python exception and Shell `exit` is reliable there. **The interactive REPL path
still hangs** at the current 96 KB budget.

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
