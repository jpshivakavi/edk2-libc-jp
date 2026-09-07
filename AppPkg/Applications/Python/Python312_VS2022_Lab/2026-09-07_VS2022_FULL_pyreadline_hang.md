# Lab: VS2022 FULL pyreadline — Shell `exit` hang reproduces (2026-09-07)

**Branch:** `feature/python-3.12.13-vs2022` · **Code state:** **`3afa03f5`** (same state pinned by
`python312-unified-full-lab-2026-09-04`; all commits since are docs-only)
**Toolchain:** **VS2022 FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY`
**Procedure:** [`../Python312_Smoke_Tests.md`](../Python312_Smoke_Tests.md) **§5.0** phases 1–5
**Result:** **pyreadline hang REPRODUCES on VS2022** — confirms the historical failure at the
current pinned code state. Manufacturing stdio policy stands.

---

## Observed

| Phase | Content | Result |
|------:|---------|--------|
| **1** | Stub default, env unset (§5.2) | **Pass** — teardown clean, Shell `exit` → firmware, **no hang** |
| **2** | Non-interactive opt-in (§5.3) | **Test passes**, then **Shell `exit` HANGS** |
| **3** | Interactive (§5.4) | `import readline` OK; **Up/Down arrow history works** after typing statements; then **Shell `exit` HANGS** returning to BIOS setup |
| **5** | Clear env, re-confirm stub (§5.6) | **Pass** — `exit` clean, **no hang** |

Phase 4 was not reported. Tab completion in phase 3 was not reported — history recall via
Up/Down was.

---

## What this narrows down

**1. The hang is pyreadline-specific, not a teardown regression.** Phases 1 and 5 both tear
down cleanly on the *same image*, differing only in whether the real readline path loaded. So
`edk2_console_detach_readline`, interpreter finalize and the MSVC 368 entry path are all fine in
the default configuration — nothing here contradicts the 2026-09-04 sign-off.

**2. Interactive use is NOT required to trigger it — this is the key new detail.** Phase 2 hung
with **no arrow keys pressed and no interactive REPL**, only a one-shot
`-S -c "import readline, …"`. The historical write-ups describe this as a *REPL* / line-editing
problem; it is not. **Merely importing the real `readline` module is sufficient.** At import,
`readline.py` runs:

```python
console.install_readline(rl.readline)   # -> edk2console.install_readline_hook
...
rl.read_history_file()
```

so hook install alone poisons the Shell `exit` path. That makes the repro far cheaper than an
interactive session and rules out keystroke handling as the cause.

**3. Python exits cleanly; the firmware handoff does not.** In both failing phases the
interpreter returned to **`Shell>`** normally. Only the subsequent Shell **`exit`** to
firmware/setup hangs. So the defect is in what is left behind for the Shell, not in Python
shutdown.

**4. It does not survive a power-cycle.** Phase 5 was clean on the following boot, so whatever
state is corrupted is not persisted to NVRAM.

---

## Trace result (captured 2026-09-07, phase 2, KVM console)

**Full teardown ladder completed.** Transcribed from the console:

```text
Python312 boot: Py_RunMain after pymain_run_python
Python312 boot: edk2_console_detach_readline enter          <-- call #2 (post-import)
Python312 boot: stop_timer: already off
Python312 boot: edk2_console_detach_readline leave
Python312 boot: Py_RunMain after edk2_console_detach_readline
Python312 boot: Py_FinalizeEx enter
Python312 boot: Py_FinalizeEx after wait_for_thread_shutdown
Python312 boot: Py_FinalizeEx after _PyAtExit_Call
Python312 boot: Py_FinalizeEx after flush_std_files
Python312 boot: finalize_modules enter
Python312 boot: edk2_console_detach_readline enter          <-- call #3 (py_console_free)
Python312 boot: stop_timer: already off
Python312 boot: edk2_console_detach_readline leave
Python312 boot: finalize_modules leave
Python312 boot: Py_FinalizeEx after finalize_modules
Python312 boot: Py_FinalizeEx before call_ll_exitfuncs
Python312 boot: Py_FinalizeEx after call_ll_exitfuncs
Python312 boot: Py_FinalizeEx leave
Python312 boot: Py_RunMain after Py_FinalizeEx
Python312 boot: after Py_BytesMain
Python312 boot: after main()
Python312 boot: after ShellCEntryLib
Python312 boot: after edk2_free_environ
Python312 boot: before return from UefiMain
FS1:\EFI\bin\>
```

### What this eliminates

| Ruled out | Evidence |
|-----------|----------|
| **Python-side teardown** | Every stage printed through **`before return from UefiMain`**, and the prompt returned. Nothing in Python or `UefiMain` hangs |
| **The 1 ms periodic timer** | **`stop_timer: already off`** at *both* detach calls — the timer was **never created**. `py_console_install_readline_hook` deliberately does not start it, with a comment saying it "can hang the next Shell command or Shell exit". **That known cause is already mitigated, so this is a different one** |
| **`detach_readline` itself** | `enter`/`leave` on all three calls; none hangs |
| **`edk2_console_handoff_to_shell`** | **No trace lines** — it never ran. Its only two call sites are `py312_openssl_uefi.c:32` (gated on `py312_openssl_loaded`, and this run did not import `ssl`) and `edk2console.c:606` (Python-level `shutdown_interactive`) |
| **ConInEx being left open** | `PY_UEFI_PYREADLINE` is **not** among the `Python312.inf` MSFT flags (only `PY_UEFI_BOOT_TRACE=1`, `PY_UEFI_MSVC_368_ENTRY=1`, `BUILD_PYTHON312_FULL=1`), so `UefiMain` never opened it. `edk2_console_ensure_input()` has exactly one call site — `py_console_getkeys` — which never ran in a non-interactive run. **Caveat:** inferred, not observed — see instrumentation gap below |
| **ConOut being reconfigured** | `Console.__init__` only **reads** via `get_output_mode_ex()`; nothing wrote through `edk2console` in phase 2 |

**Conclusion: the hang is entirely outside the Python image.** Python exits, `UefiMain`
returns, the prompt comes back — and only the Shell's own `exit` to firmware hangs afterwards.
Whatever is left behind survives image exit.

### Code defects found while analysing (independent of the hang)

| # | Finding |
|---|---------|
| 1 | **`edk2_console_restore_for_shell()` is never called from anywhere** — declared in both copies of `edk2console_api.h`, defined at `edk2console.c:178`, **zero call sites**. Dead API |
| 2 | **No ConOut restore exists anywhere in the tree**, despite two API contracts promising it. The header says `restore_for_shell` is *"ConOut restore only"*, but it forwards to `edk2_console_restore_firmware_console()`, whose entire body is `edk2_console_drain_input()` — which touches **ConIn**, not ConOut. `edk2_console_handoff_to_shell` is documented *"restore ConIn/ConOut"* and likewise only detaches and drains |
| 3 | `py_console_install_readline_hook` stores **`py_console_readline_hook = hook`** with **no `Py_INCREF`** — a borrowed reference held in a C global. Latent refcount bug; not this hang (cleared at detach) |

Defect 2 matters most for **phase 3**, where pyreadline *does* write through `edk2console`
(`set_output_attr`, `puts`, `set_cursor_pos`) and nothing puts ConOut back.

### Instrumentation gap — one line would close it

`edk2_console_detach_readline()` has **no trace inside** its
`if (g_edk2_globals.console_in != NULL)` branch, so the ladder cannot prove whether
`CloseProtocol` ran. Adding a `PY312_CONSOLE_TRACE` there (and an `else`) would settle whether
ConInEx was ever open — currently inferred, not observed.

## Bisect result (2026-09-07) — `edk2console` is exonerated; the trigger is pure-Python import

| Run | Result |
|-----|--------|
| `import edk2console; print('ok')` | **`ok`, Shell `exit` reaches BIOS setup — clean** |
| `import pyreadline.rlmain; print('ok')` | **`ok`, then Shell `exit` HANGS** |
| `import edk2console; edk2console.install_readline_hook(...)` | not needed — see below |

The third run is now **moot**, because run 2 hangs *without installing any hook*. Verified by
reading the tree: **`rl = Readline()` and `console.install_readline(rl.readline)` are both in
`readline.py` (lines 55 and 99)** — neither `pyreadline/__init__.py` nor `rlmain.py` has a
module-level `Readline()` or `install_readline` call.

So `import pyreadline.rlmain` provably did **none** of this:

- did **not** construct `Readline()`, so no `Console()`, so **no `get_output_mode_ex()` call**
- did **not** call `install_readline` → `install_readline_hook`, so `PyOS_ReadlineFunctionPointer`
  and `py_console_readline_hook` were **never set**
- did **not** call `read_history_file()`
- did **not** call `getkeys`, so `edk2_console_ensure_input()` never ran

**Every `edk2console` C entry point is ruled out.** The only C code that executed in *both* runs is
`PyInit_edk2console` (a bare `PyModule_Create`) plus `py_console_free` at finalize — and run 1
proves that pair is harmless. `py_console_get_output_mode_ex` is pure reads of `ConOut->Mode`
fields with no side effects, and it was never called anyway.

**What is left is only the module-level import work**, which for
`pyreadline/__init__.py` is, in order:

```python
from . import unicode_helper
from . import logger, lineeditor, modes, console
from .rlmain import *
from . import rlmain
```

`logger.py` pulls the **stdlib `logging` package** (which drags in `threading`, `weakref`,
`atexit` registration, `collections.abc`, `string`, `re`); `console/edk2.py` pulls `traceback`,
`re`, `keysyms`, and `console/ansi`. Roughly 38 `.py` files are read from the FAT volume.
`logger.py` imports `socket` only **inside** `SocketStream.__init__`, which never runs — so the
known `socket.py`/`selectors` teardown failure is **not** in play here.

**This most likely is not a readline bug at all.** It now looks like "import enough pure Python —
or specifically `logging` — and Shell `exit` hangs", which would affect any script, not just
pyreadline.

### Next step — stdlib controls first, then a rebuild-free bisect

Run these and Shell `exit` after each. They need **no `PY_UEFI_READLINE`** and **no rebuild**:

```text
Python312.efi -S -c "import logging; print('ok')"
Python312.efi -S -c "import re, traceback; print('ok')"
Python312.efi -S -c "import logging, re, traceback; print('ok')"
```

| Outcome | Meaning |
|---------|---------|
| `logging` alone **hangs** | **pyreadline is a red herring.** The bug is in the `logging` import (its `atexit` registration, `threading`/`weakref` use) and is far broader than readline |
| Only the combined run hangs | Cumulative import volume / pool pressure, not one module |
| **All three clean** | The trigger is pyreadline-specific — proceed to the in-place bisect below |

**The in-place bisect costs no rebuild**, because `pyreadline/` is pure Python staged on the
volume at `EFI\lib\python3.12\pyreadline\`. Edit `__init__.py` **on the stick** and comment the
import lines progressively, running `import pyreadline; print('ok')` + Shell `exit` each time:

| Step | Leave uncommented | Isolates |
|------|-------------------|----------|
| A | `unicode_helper` only | baseline |
| B | + `logger` | stdlib `logging` |
| C | + `lineeditor` | history/lineobj |
| D | + `modes` | keybinding tables |
| E | + `console` | `edk2console` + `ansi` + `keysyms` |
| F | + `rlmain` | full package (known-hanging state) |

The first step that hangs names the culprit, in at most six boots.

Note that dotted imports **cannot** bisect this — `import pyreadline.logger` executes the whole
`pyreadline/__init__.py` first, which is why the file must be edited in place.

### Superseded plan — original three-run bisect

Phase 1 (stub) also imports `readline.py` from FS1 and exits cleanly, so the delta is narrow:
the **pyreadline package import** (~38 files), **`rlcompleter`**, **`rl.read_history_file()`**,
and **`install_readline_hook`**. These three runs separate them — each followed by Shell `exit`:

```text
Python312.efi -S -c "import edk2console; print('ok')"
Python312.efi -S -c "import pyreadline.rlmain; print('ok')"
Python312.efi -S -c "import edk2console; edk2console.install_readline_hook(lambda p: chr(10)); print('ok')"
```

| Run | If `exit` hangs | If `exit` is clean |
|-----|-----------------|--------------------|
| 1 — `edk2console` only | The C module's mere presence/init is enough | Module init is innocent |
| 2 — full pyreadline tree, **no** hook | Cause is the **package import / FAT file I/O**, not the hook | File I/O is innocent → suspect the hook |
| 3 — hook installed, **no** pyreadline | Cause is **`install_readline_hook`** | Hook is innocent → suspect import/history |

Run 2 does **not** need `PY_UEFI_READLINE`, since it bypasses `readline.py`'s gate entirely.

---

## Original diagnostic instructions (superseded by the result above)

## Next diagnostic — instrumentation is already in the image

**`/DPY_UEFI_BOOT_TRACE=1` is already on `MSFT:*_*_*_CC_FLAGS` in `Python312.inf`**, so
`PY312_CONSOLE_TRACE` (`edk2console.c`) and `py312_boot_print_ascii` are **live in the image
already tested** — no rebuild needed.

### There are TWO `detach_readline` calls — check the second one

This is the trap that makes a naive reading useless. `edk2_console_detach_readline()` runs at
**startup** and again **after** user code:

| # | Call site | When |
|---|-----------|------|
| **1** | `python.c` `main()`, before `Py_BytesMain` | Always, at startup — **harmless, always prints `enter`/`leave`** |
| **2** | `main.c` `Py_RunMain()`, after `pymain_run_python()` | **The one that matters** — the only detach that runs *after* `import readline` installed the hook |

So "`enter` and `leave` appeared" proves nothing on its own. Anchor on the line
**`Py_RunMain after pymain_run_python`** and read only what follows it.

### Expected ladder (phase 2, `-S -c "import readline …"`)

```text
Python312 boot: main enter
Python312 boot: after _PyMem_SetupAllocators
Python312 boot: edk2_console_detach_readline enter        <-- call #1 (startup, ignore)
Python312 boot: stop_timer: already off
Python312 boot: edk2_console_detach_readline leave
Python312: enter main
Python312 boot: before Py_BytesMain
Readline True                                             <-- script output
Python312 boot: Py_RunMain after pymain_run_python         <-- ANCHOR: read from here down
Python312 boot: edk2_console_detach_readline enter        <-- call #2 (post-import)
Python312 boot: stop_timer: ...
Python312 boot: edk2_console_detach_readline leave
Python312 boot: Py_RunMain after Py_FinalizeEx
Python312 boot: after Py_BytesMain
Shell>
```

### Decision table — applies to call #2 only

| After the anchor line | Interpretation | Where to look |
|-----------------------|----------------|---------------|
| `enter` **and** `leave` present | Detach completed. The leak is **after** it — handle/ConIn state left for the Shell, or an event never closed. **Most likely**, since Python reached `Shell>` | `CloseProtocol` semantics; whether the Shell's own ConInEx consumer is disturbed; `edk2_console_handoff_to_shell` is **not** on this path |
| `enter` but **no** `leave` | Hang is **inside** detach — `edk2_console_drain_input()`'s `while (ReadKeyStrokeEx…)` loop, or `CloseProtocol`. Contradicts reaching `Shell>`, so would be a surprise worth reporting | `edk2console.c` lines 100–160 |
| **Anchor line present, no `enter` at all** | Detach did **not** run post-import — the `#ifdef UEFI_C_SOURCE` block was compiled out or skipped | `main.c` ~730 |
| **Anchor line absent** | `Py_RunMain` never reached that point | `pymain_run_python` |
| `stop_timer: already off` at call #2 | The `getkeys` **timer was never created** — so the timer is **not** the culprit for a non-interactive hang | — |
| `stop_timer: TimerCancel` / `CloseEvent` / `done` at call #2 | A periodic timer **was** live and got cancelled — compare with phase 3, where interactive use is more likely to have started it | `edk2_console_start_timer` |

**Comparing phase 1 with phase 2 is the highest-value signal.** Phase 1 exits cleanly, so any
line present at call #2 in phase 2 but absent in phase 1 is the delta that breaks Shell `exit`.

**Known dead end — do not retry blind:** `edk2console.c` documents that resetting ConIn does
not help, and both directions were already tried:

> *"Do not Reset ConIn here; `Reset(TRUE)` hangs after REPL exit and `Reset(FALSE)` has hung the
> next Shell launch on VS2022. Drain only."*

So the fix is not a ConIn reset. Let the trace decide the direction before changing code.

### Capture

The trace lines are printed by `Print()` to **ConOut**, so shell `>` redirection is not
dependable for them. They are, however, the **last lines before the `Shell>` prompt**, so they
are on screen when the prompt returns — read them before typing `exit`. Serial / BMC console
capture is best if available; photographing the screen is an acceptable fallback.

---

## Status

**VS2022 pyreadline: NOT signed off — hang confirmed at `3afa03f5`.** Manufacturing default
remains **stdio**, and this run is positive evidence for that policy rather than a new defect.

**GCC is unaffected** — phases 1–5 all pass at the same code state
([`2026-09-07_GCC_FULL_pyreadline_phases.md`](./2026-09-07_GCC_FULL_pyreadline_phases.md)),
which keeps this firmly in the **GCC-vs-VS2022 divergence** category (deviations §11.3): the two
toolchains use different firmware entry paths, and this is a runtime difference from the **same
commit**.

**Operator note:** use **`set -v`** so the variable clears on the power-cycle each hang forces;
plain `set` is non-volatile and would persist (§5.6).
