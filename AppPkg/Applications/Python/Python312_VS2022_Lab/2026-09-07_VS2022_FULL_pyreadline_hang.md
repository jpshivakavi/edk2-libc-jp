# Lab: VS2022 FULL pyreadline — Shell `exit` hang reproduces (2026-09-07)

**Branch:** `feature/python-3.12.13-vs2022` · **Code state:** **`3afa03f5`** (same state pinned by
`python312-unified-full-lab-2026-09-04`; all commits since are docs-only)
**Toolchain:** **VS2022 FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY`
**Procedure:** [`../Python312_Smoke_Tests.md`](../Python312_Smoke_Tests.md) **§5.0** phases 1–5
**Result:** **pyreadline hang REPRODUCES on VS2022** — confirms the historical failure at the
current pinned code state. Manufacturing stdio policy stands.

> **ROOT CAUSE FOUND (2026-09-08) — see "ROOT CAUSE" below.** VS2022 images set
> `PY_UEFI_MSVC_368_ENTRY`, which makes `edk2main.c` **return early before the stack switch**, so
> the interpreter runs on the ~128 KB **firmware stack**; GCC takes the other branch and gets a
> **64 MB** `malloc`'d stack. Deep import chains overflow the firmware stack, corrupting memory
> *outside* the image — which is why Python's own teardown looks flawless and only the Shell's
> `exit` into BDS hangs. `PyOS_CheckStack()` cannot catch it because `g_edk2_globals.stack` is
> NULL on that path, so it always answers "stack is fine".
>
> **SCOPE CHANGED — read this too.** The filename says "pyreadline", but the bisect in this note
> proved that wrong. **`Python312.efi -S -c "import logging; print('ok')"` alone hangs Shell
> `exit`.** There is no readline, no `edk2console`, and no console I/O in that command. Both
> hanging cases share exactly one heavyweight import — **`logging`** (pyreadline reaches it via
> `pyreadline/logger.py`). Treat this note as **"VS2022 FULL: pure-Python import hangs Shell
> `exit`"**; pyreadline is just how it was first noticed. The filename is kept so existing
> cross-references stay valid.

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

## Stdlib control result (2026-09-07) — `import logging` alone hangs

```text
Python312.efi -S -c "import logging; print('ok')"      -> ok, then Shell exit HANGS
```

**pyreadline is entirely out of the picture.** No readline, no `edk2console`, no console I/O.
And it explains the earlier runs: `pyreadline/logger.py` imports `logging`, so **both** hanging
cases share exactly that one heavyweight import, while every clean case avoids it.

| Command | `logging` imported? | Shell `exit` |
|---------|---------------------|--------------|
| `import edk2console` | no | **clean** |
| `import zlib, ssl, ctypes, hashlib` (phase 8) | no | **clean** |
| stub `import readline` (phase 1) | no | **clean** |
| `import pyreadline.rlmain` | **yes** (via `logger.py`) | **HANG** |
| `import logging` | **yes** | **HANG** |

### Threading is ruled out by inspection — no rebuild needed to know this

`import logging` does create a lock at import time — `logging/__init__.py:232` is
`_lock = threading.RLock()` — and registers an exit hook at `:2280-2281`
(`import atexit; atexit.register(shutdown)`). That made `_thread` the obvious suspect, but the
build's pthread layer rules it out:

- `pyconfig.h` sets **`_POSIX_THREADS 1`** and **`HAVE_PTHREAD_H 1`**, so `Python/thread.c`
  selects `thread_pthread.h`
- the pthread symbols come from **`PyMod-3.12.13/efi/src/dummy_pthread.c`**, which is
  **pure static-array bookkeeping** — `mutexes[256]`, `conds[256]`, `keys[256]`, handing out
  pointers into those arrays

There is **no `gBS` call, no `CreateEvent`, no allocation and no firmware state anywhere** in
`dummy_pthread.c`. A leftover Python lock therefore cannot influence the Shell. The `atexit` hook
is also cleared, since the boot trace shows `Py_FinalizeEx after _PyAtExit_Call`.

**With console I/O, hooks, timers, events and locks all eliminated, the only thing a cleanly
exited image can still have left behind is heap/pool footprint.** Leading hypothesis: Shell
`exit` returns to BDS, which needs to allocate to bring up the setup UI, and the memory the
interpreter never returned makes that hang. This fits every data point above — the clean commands
are all small or C-extension-only, and the hanging ones both pull the large pure-Python
`logging` tree.

## `import json` also hangs (2026-09-07) — `logging` is out too; `re` now correlates perfectly

```text
Python312.efi -S -c "import json; print('ok')"        -> ok, then Shell exit HANGS
```

`json` imports neither `logging` nor `threading`, so **`logging` was never special either.**
Verified by reading the tree: `json/decoder.py`, `json/encoder.py` and `json/scanner.py` all
`import re` (line 3 of each), and `logging/__init__.py:26` imports `re` as well.

| Command | pure-Python modules pulled | `re`? | Shell `exit` |
|---------|---------------------------|-------|--------------|
| `import edk2console` | none | no | **clean** |
| `import zlib, ssl, ctypes, hashlib` (phase 8) | very few — the staged `Lib/ssl/__init__.py` imports **only `os`** | no | **clean** |
| stub `import readline` (phase 1) | 1 | no | **clean** |
| `import logging` | ~20 | **yes** | **HANG** |
| `import json` | ~20 | **yes** | **HANG** |
| `import pyreadline.rlmain` | ~38 | **yes** (via `logger.py` → `logging`) | **HANG** |

**`re` is now perfectly correlated with the hang**, and it drags in `enum`, `functools`,
`collections`, `types`, `operator`, `reprlib` and `copyreg`. Note this also explains why phase 8
always looked clean: this port's `ssl` is the UEFI-minimal variant whose `__init__.py` imports
nothing but `os`, so the FULL smoke tests barely touch the pure-Python stdlib at all.

But `re` being correlated does not yet distinguish *the module* from *the volume of imports* —
~20 modules is also simply ~20 more file opens and a few hundred KB more heap than any clean run.

## Mechanism runs (2026-09-07) — all three CLEAN; raw bytes and file cycles both eliminated

```text
Python312.efi -S -c "import re; print('ok')"                                    -> clean
Python312.efi -S -c "x = bytearray(16*1024*1024); print('ok')"                  -> clean
Python312.efi -S -c "[open('Python312.efi','rb').close() for i in range(50)]"   -> clean
```

Three more mechanisms are now dead:

| Eliminated | By |
|------------|-----|
| **`re` itself** | `import re` is clean, so `re` and its whole tree — `enum`, `functools`, `collections`, `abc`, `reprlib`, `types`, `operator`, `copyreg`, `_sre` — are **not** sufficient |
| **Raw heap footprint** | A single **16 MB** `bytearray` is clean. That is far more memory than ~20 stdlib modules consume, so **total bytes is not the metric** |
| **File open/close volume** | 50 `open`/`close` cycles are clean, so the StdLib/FAT file layer does **not** leak per open |
| **`_thread` / locks (again, now empirically)** | `import re` pulls `functools`, whose line 21 is `from _thread import RLock` — so `_thread` **is** loaded in a **clean** run. This confirms the `dummy_pthread.c` reading with hardware evidence |

### The remaining margin is razor thin, which points at a threshold

`import re` already loads roughly 15+ modules and is clean. `import json` adds only about five on
top — `json`, `json.decoder`, `json.encoder`, `json.scanner`, `_json` — and hangs. Something is
being crossed in a very narrow band, and since raw bytes are ruled out, the candidates are:

1. **Allocation count / pool fragmentation** — importing modules makes tens of thousands of
   *small* long-lived objects, so Python's allocator requests many arenas from the EFI pool. One
   16 MB block is a completely different allocation *shape* and does not test this at all.
2. **A count-based limit** on modules, arenas, or some fixed-size table.
3. **C-stack depth** — import machinery recurses through C, unlike the flat one-liners that pass.
   Worth noting this port has a stack-switch entry path (`PY_UEFI_MSVC_368_ENTRY`), so stack
   headroom is not the firmware default.

## Threshold measured (2026-09-08) — boundary is 43–48 modules

| Command | `len(sys.modules)` | Shell `exit` |
|---------|--------------------|--------------|
| `import sys` (baseline, `-S`) | **23** | **clean** |
| `import re, sys` | **42** | **clean** |
| `import json, sys` | **48** | **HANG** |
| `import logging, sys` | **65** | **HANG** |

The boundary is **between 42 and 48 total modules**, i.e. between **19 and 25 modules newly
imported** past the `-S` baseline. It is **not** a descriptor limit: `StdLib/Include/sys/syslimits.h:56`
sets **`OPEN_MAX 255`** (and `FOPEN_MAX` follows it), so nothing runs out at 20-odd files.

## The `.pyc` write path — a real gap in the earlier testing

**The 50-cycle file test used `open(..., 'rb')` — read-only.** It never created, wrote, renamed or
deleted anything, so it did **not** clear the write path. And this port writes on every import:

- **`create_python_pkg.sh` deliberately excludes `__pycache__`** (lines 107, 117, 136), so the
  staged volume ships with **no precompiled bytecode at all**
- `importlib/_bootstrap_external.py:1141` gates on `not sys.dont_write_bytecode`, so writing is
  **enabled** by default
- `:1235` creates the `__pycache__` directory, then `:1245` calls `_write_atomic(path, data, _mode)`
- `_write_atomic` (`:201`) writes a **`.tmp`** sibling and then calls **`_os.replace(path_tmp, path)`**;
  on `OSError` it tries `_os.unlink(path_tmp)` and re-raises

So each stdlib import performs a directory create, a temp-file create, a write, and a **rename**
on the FAT volume through the firmware. Roughly 19 such writes happen in the clean `re` run and
about 25 in the hanging `json` run — which straddles the measured boundary. Unflushed or
half-completed FAT mutation is exactly the kind of residue that survives image exit and could
stall a volume teardown when the Shell exits.

If `_os.replace` is not properly supported on this port, the `.tmp` files may also be orphaned,
adding directory churn.

## `-B` result (2026-09-08) — bytecode writing is NOT the mechanism

```text
Python312.efi -B -S -c "import json; print('ok')"     -> ok, then Shell exit HANGS
```

`json` is unchanged and the only removed variable was the `__pycache__` write, so **`.pyc`
writing, `__pycache__` mkdir, `_write_atomic` and `_os.replace` are all cleared.** One boot,
hypothesis dead. (The packaging observation still stands on its own — the volume ships without
bytecode, so every launch pays full compilation — but it is a performance note, not this bug.)

### Stack depth is now unlikely, by reasoning

Import nesting does **not** grow with module *count*: `json` → `json.decoder` → `re` → `enum` is
about as deep as `re` → `enum` alone. A threshold that trips between 42 and 48 modules is a
**cumulative** effect, and C-stack depth is not cumulative across sibling imports. Keep the probe
in the list, but expect it to be clean.

### Leading hypothesis: allocation count / pool fragmentation

This is the one mechanism that scales cumulatively with module count and that **none** of the
clean runs tested:

- the 16 MB `bytearray` is a **single** large `malloc` — big objects bypass `obmalloc` entirely,
  so it created **no arenas** and left **one** hole in the EFI pool
- ~48 modules create **tens of thousands of small long-lived objects**, so `obmalloc` requests
  **many separate arenas** via `AllocatePool`, scattered across the pool

If the image exits without returning them, the firmware pool is left **fragmented** rather than
merely smaller — and BDS then needs a large contiguous allocation to bring up the setup UI after
Shell `exit`. That distinction explains why one 16 MB block is harmless while ~5 MB spread over
many arenas is not.

## ROOT CAUSE (2026-09-08) — VS2022 runs Python on the firmware stack; GCC gets 64 MB

The user's observation that **GCC never shows this hang** is the key that cracks it. The two
toolchains take **completely different entry paths**, in `PyMod-3.12.13/efi/src/edk2main.c`:

```c
#if defined(_MSC_VER) && defined(PY_UEFI_MSVC_368_ENTRY)     /* :202 */
   status = ShellCEntryLib(image, systab);                    /* firmware stack, no switch, no IDT */
   ...
   return status;                                             /* :212 early return */
#endif

   g_edk2_globals.stack_size = PY_UEFI_DEFAULT_STACK_SIZE;    /* :215  = 64 MB */
   g_edk2_globals.stack = malloc(g_edk2_globals.stack_size + 1024);
   ...
   edk2_switch_stack(aligned_stack, g_edk2_globals.stack_size);  /* :228 */
   py_install_idt();                                             /* :231 */
   status = ShellCEntryLib(image, systab);
```

`PY_UEFI_DEFAULT_STACK_SIZE` is **`(64*1024*1024)`** — 64 MB (`efi/Include/efi/edk2stack.h:5`).
And `PY_UEFI_MSVC_368_ENTRY=1` is on the MSFT `CC_FLAGS` of **both** `Python312.inf:1073` **and**
`Python312_MIN.inf:327`.

**So every VS2022 image runs the entire interpreter on the UEFI firmware stack — on the order of
128 KB for a Shell application — while GCC runs it on a 64 MB switched stack. Roughly a 500×
difference in stack headroom.**

### Second defect: the guard that should have caught this is broken on exactly that path

```c
int                                          /* edk2main.c:249 */
PyOS_CheckStack(void)
{
   uint64_t rsp = edk2_read_rsp();
   if(rsp > (uint64_t)g_edk2_globals.stack)
      return 0;                              /* "stack is fine" */
   return 1;
}
```

`g_edk2_globals.stack` is assigned at `:216` — **after** the 368 early return at `:212`. In a
VS2022 image it is therefore **NULL**, `rsp > 0` is always true, and `PyOS_CheckStack()`
**unconditionally reports that there is plenty of stack**. This is not dead code:
`USE_STACKCHECK 1` is defined at `PyMod-3.12.13/efi/Include/pyconfig.h:1816`, and the function is
called from `Objects/object.c`, `Python/ceval.c:260` and `Python/pythonrun.c:1913`. **The guard is
wired up and actively lying**, so deep recursion runs off the bottom of the firmware stack
silently instead of raising `RecursionError`.

### Why this explains every single observation

| Observation | Explanation |
|-------------|-------------|
| **GCC never hangs** | 64 MB switched stack — nowhere near overflow |
| **Python's teardown trace is flawless** | The overflow corrupts memory *below the firmware stack*, outside the image. Python itself is undamaged, finalizes correctly, and `UefiMain` returns — precisely what the captured ladder shows |
| **Hang only at Shell `exit`** | BDS regains control and touches the corrupted region to bring up the setup UI |
| **`import re` (42) clean, `import json` (48) hangs** | `json` is a **package**, so its chain is deeper: `json/__init__` → `json.decoder` → `re` → `_compiler` → `_parser` → `_constants`, about two levels more than `import re`. **Peak C-stack depth is the real variable** |
| **Sharp threshold** | A stack limit is a hard boundary, unlike gradual pressure |
| **16 MB `bytearray` clean** | Heap, and shallow |
| **50 `open`/`close` clean** | Shallow |
| **`-B` made no difference** | Irrelevant to stack usage |

**Correction to my earlier framing:** module count was only ever a proxy. The real variable is
**peak C-stack depth**, which correlates with count merely because larger imports nest deeper. My
earlier claim that "depth is not cumulative, so stack is unlikely" was wrong — it ignored that
nested *packages* genuinely add frames.

### Why the workaround exists — a boot hang was traded for an exit hang

`edk2main.c:203-205`: *"Python 3.6.8 AppPkg: ENTRY_POINT = ShellCEntryLib on the firmware stack
(no edk2_switch_stack / py_install_idt). VS2022 3.12 hang reproduces inside ShellCEntryLib only
after stack switch — try 368-style path."*

`Python312.inf:1070-1072`: *"FULL VS2022: keep PY_UEFI_MSVC_368_ENTRY — GCC stack+IDT hangs inside
ShellCEntryLib (boot stops at 'before ShellCEntryLib')."*

So the 368 path was adopted to cure a **boot** hang, and unknowingly traded it for this **exit**
hang. Importantly, `edk2stack.nasm` and `edk2handler.nasm` are listed in `[Sources]` at
`Python312.inf:62` and `:69` with **no `| MSFT` / `| GCC` restriction**, so `edk2_switch_stack` is
already compiled and linkable in the VS2022 image — **the assembly is not the blocker.**

### Fixes, cheapest first

1. **Decouple the stack switch from the IDT install.** The comments blame "stack+IDT" as a pair,
   but `edk2_switch_stack()` (`:228`) and `py_install_idt()` (`:231`) are independent calls. Build
   a VS2022 image that switches the stack and **skips `py_install_idt()`**. If it boots, VS2022
   gets the 64 MB stack and this bug disappears — this is the real fix.
2. **Fix `PyOS_CheckStack` for the 368 path** regardless of the above: capture the firmware stack
   base at entry and compare against that instead of a NULL `g_edk2_globals.stack`. This converts
   silent firmware-memory corruption into a clean `RecursionError`. It does not raise the ceiling,
   but a guard that always answers "fine" is a correctness bug on its own.
3. **Stopgap without a rebuild:** lower `sys.setrecursionlimit()` so Python's own counter trips
   before the firmware stack is exhausted. Crude and approximate, since the limit counts Python
   frames rather than C frames.

### Fix 2 implemented (2026-09-08) — `PyOS_CheckStack()` now has a real bound

Chosen deliberately as the *safety* fix: it does **not** raise the ceiling, it converts silent
firmware-memory corruption into a clean, catchable Python error. The 368 entry path is untouched,
so no boot-path risk.

| File | Change |
|------|--------|
| `PyMod-3.12.13/efi/Include/efi/edk2main.h` | New `uint64_t stack_limit` in `edk2_globals_t` — lowest safe `rsp`, margin already applied, `0` meaning "unknown" |
| `PyMod-3.12.13/efi/Include/efi/edk2stack.h` | New `PY_UEFI_STACK_MARGIN` (8 KB) and `PY_UEFI_FIRMWARE_STACK_BUDGET` (96 KB), both `#ifndef`-guarded so they can be overridden from `CC_FLAGS` |
| `PyMod-3.12.13/efi/src/edk2main.c` | **368 path:** derives `stack_limit` from `rsp` at entry, since no switch happens. **GCC path:** sets it from the `malloc`'d stack base plus the margin, and clears it to `0` after `edk2_revert_stack()` so a stale bound can never be compared against freed memory. **`PyOS_CheckStack()`** now returns `0` when `stack_limit` is `0` (unknown → old permissive behaviour) and otherwise `rsp <= stack_limit` |

```c
int
PyOS_CheckStack(void)
{
   uint64_t limit = g_edk2_globals.stack_limit;

   if(limit == 0)
      return 0;

   return edk2_read_rsp() <= limit;
}
```

**Expected new behaviour on VS2022:** the deep-import cases should now raise
`MemoryError: Stack overflow` — the string comes from `_Py_CheckRecursiveCall()` in
`Python/ceval.c:263` — **instead of hanging the Shell**. That is the fix working, not a
regression. `import re` should stay clean.

**Verify the bound is actually set.** A new trace line prints under the existing
`PY_UEFI_BOOT_TRACE=1`, so no flag change is needed:

```text
Python312 boot: firmware stack rsp=... limit=... budget=18000
```

A non-zero `limit` confirms the fix is live in the image.

**Tuning the budget.** 96 KB is a guess at a safe fraction of the firmware stack, which UEFI does
not expose to applications:

| Symptom after this change | Meaning | Action |
|---------------------------|---------|--------|
| Shell `exit` **still hangs** | Budget too large — the overflow happens before the check trips | Lower it, e.g. `/DPY_UEFI_FIRMWARE_STACK_BUDGET=49152` |
| `MemoryError` on scripts that should fit | Budget too small | Raise it |
| Deep imports raise `MemoryError`, `exit` clean | **Working as intended** | Then pursue fix 1 to actually regain depth |

**This does not make `json`/`logging` usable on VS2022** — that still needs fix 1 (the stack
switch). It makes the failure *safe and diagnosable* rather than firmware-corrupting.

### Confirming test — depth versus count, no rebuild

Stage tiny modules on the volume and compare:

- **deep and few:** `t1.py` contains `import t2`, `t2.py` contains `import t3`, … `t10.py` is
  `x = 1`. Then run `import t1` — **10 modules, depth 10**.
- **shallow and many:** `u1.py` … `u30.py` each containing `x = 1`. Then run
  `import u1, u2, ... u30` — **30 modules, depth 1**.

If deep-and-few hangs while shallow-and-many stays clean, **depth is confirmed** as the variable
and count is exonerated. This is the cleanest possible separation and needs only text files on
the stick.

### Superseded plan — allocator and `memmap` investigation

**`memmap` is the highest-value run here, because it observes the firmware directly and the hang
does not block it** — the prompt returns fine, so run `memmap` *before* typing `exit`:

```text
memmap
Python312.efi -S -c "import re; print('ok')"
memmap
Python312.efi -S -c "import json; print('ok')"
memmap
```

Compare free-page totals and descriptor counts across the three. A clean run and a hanging run
that differ in **fragmentation** (many more, smaller free regions) rather than in total free
memory would confirm the hypothesis from the firmware's own accounting. `memmap` ships in the
Debug1 profile; if this Shell lacks it, `dh` or `smbiosview` are not substitutes — skip to the
allocator measurements below.

**Quantify the allocation delta at the boundary** — the small-object analogue of the
`len(sys.modules)` measurement, which bracketed so well:

```text
Python312.efi -S -c "import sys; print(sys.getallocatedblocks())"
Python312.efi -S -c "import re, sys; print(sys.getallocatedblocks())"
Python312.efi -S -c "import json, sys; print(sys.getallocatedblocks())"
```

**Then the allocation-shape test that the `bytearray` run failed to cover** — many small
long-lived objects, zero imports:

```text
Python312.efi -S -c "x = [bytes(64) for i in range(100000)]; print('ok')"
Python312.efi -S -c "x = [{} for i in range(100000)]; print('ok')"
```

| Result | Conclusion |
|--------|------------|
| either **hangs** | **Allocation count / fragmentation confirmed**, with no module involved. Fix direction becomes `obmalloc` arena behaviour against `AllocatePool` — e.g. arena size, or freeing arenas at exit |
| both **clean** | Allocation count is **not** sufficient either, which would be a strong negative — it would mean something specific to *loading modules* (code objects, `sys.modules` growth, or the import machinery's own C state) rather than raw allocation |

### Superseded plan — the `-B` test

**`-B` is supported in this build** (`PyMod-3.12.13/Python/sysmodule.c:3026` maps
`dont_write_bytecode` to `-B`), so the hypothesis can be tested with **no rebuild and no
restaging**:

```text
Python312.efi -B -S -c "import json; print('ok')"
```

| Result | Conclusion |
|--------|------------|
| **clean** | **Bytecode writing to FAT is the mechanism.** `json` itself is unchanged, so the only variable removed is the `__pycache__` write |
| still **hangs** | Writing is innocent; go back to allocation shape (below) |

A write-only control with **no imports** (note: avoid `%` in `-c`, the UEFI Shell treats it
specially):

```text
Python312.efi -S -c "[open(str(i)+'.tmp','wb').close() for i in range(30)]; print('ok')"
```

**And a zero-boot observation available right now** — from the Shell, or by reading the stick on
the host, check whether the writes actually landed:

```text
ls FS1:\EFI\lib\python3.12\json
ls FS1:\EFI\lib\python3.12\json\__pycache__
```

`__pycache__` present with `.pyc` files proves writing happens; stray **`.tmp`** files would
additionally show `_os.replace` failing on this port.

### If writing is confirmed, the fix has two shapes

1. **Pre-generate the bytecode at package time** — drop `--exclude '__pycache__/'` from
   `create_python_pkg.sh` and `compileall` the tree so imports never write. **Caveat:** the `.pyc`
   files must match the interpreter's marshal magic, so `compileall` has to run with the **same
   3.12.13** build — this port has already been bitten by magic-number mismatches.
2. **Ship with writing off** — `-B` on the command line, or `PYTHONDONTWRITEBYTECODE` in the
   environment. Cheaper, but costs re-compilation on every launch.

### Superseded plan — measure first, then test allocation shape

**Measure the boundary instead of guessing it.** These four print the module count *and* show
whether each hangs, in four boots:

```text
Python312.efi -S -c "import sys; print(len(sys.modules))"
Python312.efi -S -c "import re, sys; print(len(sys.modules))"
Python312.efi -S -c "import json, sys; print(len(sys.modules))"
Python312.efi -S -c "import logging, sys; print(len(sys.modules))"
```

The first two are known-clean and the last two known-hanging, so the numbers bracket the
threshold precisely.

Then test **allocation shape** with no imports at all — this is the run that the 16 MB block
failed to cover:

```text
Python312.efi -S -c "x = [bytes(64) for i in range(200000)]; print('ok')"
Python312.efi -S -c "x = [{} for i in range(100000)]; print('ok')"
```

If either hangs, **allocation count / fragmentation is the mechanism**, not footprint — and the
fix direction becomes the allocator's arena behaviour against `AllocatePool`, not any module.

And a one-line C-stack probe (deep parser and `repr` recursion, no imports):

```text
Python312.efi -S -c "x = eval('['*200 + ']'*200); print(len(repr(x))); print('ok')"
```

If that hangs, stack depth is implicated and the import path's recursion is the likely trigger.

### Superseded plan — the three mechanism runs above

Priority order. Each is followed by Shell `exit`; no rebuild, no `PY_UEFI_READLINE`:

```text
Python312.efi -S -c "import re; print('ok')"
Python312.efi -S -c "x = bytearray(16*1024*1024); print('ok')"
Python312.efi -S -c "[open('Python312.efi','rb').close() for i in range(50)]; print('ok')"
```

| Run | What it isolates | If it hangs |
|-----|------------------|-------------|
| `import re` | the correlated module, minimally | Confirms `re`'s tree is sufficient — then bisect it with `import enum`, `import functools`, `import collections` |
| `bytearray(16 MB)` | **memory only — zero file opens, zero stdlib imports** | **Pool/heap footprint is the mechanism.** The bug is about memory, not modules |
| 50 × `open`/`close` | **file opens only — no imports, negligible heap** | **The StdLib/FAT file layer leaks per open even when closed.** Leaked `EFI_FILE_PROTOCOL` handles keep the volume's open count non-zero, which is exactly the kind of residue that survives image exit and stalls a volume teardown at Shell `exit` |

Runs 2 and 3 are the important pair — between them they cover both remaining mechanisms with
**no module imports at all**. If run 2 is clean and run 3 hangs, the fix is in file-handle
cleanup, not memory. If both are clean, the trigger really is something `re`'s tree executes.

The `open` target works because the Shell's cwd is `FS1:\EFI\bin\`, where `Python312.efi` lives;
the list comprehension keeps it to a single `-c` line. If `bytearray` at 16 MB is clean, retry at
32 MB and 64 MB to look for a threshold — and a `MemoryError` is itself useful, as it bounds how
much pool the app can obtain.

### Superseded plan — pyreadline in-place bisect

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
