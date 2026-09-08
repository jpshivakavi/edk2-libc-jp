# Lab: VS2022 FULL — interactive `exit()` skips the console detach (2026-09-08)

**Branch:** `feature/python-3.12.13-vs2022` · **Code state:** **`4cf5698a`**
(*fix(python312): track rsp high-water mark to size the firmware stack budget*)
**Toolchain:** **VS2022 FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY`
**Result:** **Second, independent root cause found.** The interactive Shell-`exit` hang is **not**
a stack overflow. `exit()` from the REPL routes through `Py_Exit()`, which never returns through
`Py_RunMain()`, so **`edk2_console_detach_readline()` never runs** and the image exits with
**ConInEx still open on `ConsoleInHandle`**.

**Predecessors:** [`2026-09-07_VS2022_FULL_pyreadline_hang.md`](./2026-09-07_VS2022_FULL_pyreadline_hang.md)
(root cause of the *non-interactive* hang) ·
[`2026-09-08_VS2022_FULL_stackcheck_fix.md`](./2026-09-08_VS2022_FULL_stackcheck_fix.md)
(the `PyOS_CheckStack` fix, which cleared the non-interactive case).

---

## The measurement that redirected the investigation

`stack_min_rsp` / `stack_entry_rsp` instrumentation added at `4cf5698a` prints
`stack high-water min_rsp=… limit=… used=…` after `ShellCEntryLib` returns.
`entry_rsp = limit + budget = 0x6A951608 + 0x18000 = 0x6A969608`.

| Run | `min_rsp` | vs `limit` (`0x6A951608`) | `used` | Shell `exit` |
|-----|-----------|---------------------------|--------|--------------|
| `-S -c "import re"` | `0x6A952E68` | **`+0x1860`** = 6 240 B **above** — guard never fired | `0x167A0` = 91 808 B = **89.7 KB** | clean |
| `-S -c "import json"` | `0x6A94E9E8` | **`−0x2C20`** = 11 296 B below | `0x1AC20` = 109 600 B = **107.0 KB** | clean |
| interactive `>>> import json` | `0x6A94E8E8` | **`−0x2D20`** = 11 552 B below | `0x1AD20` = 109 856 B = **107.3 KB** | **HANG** |

**The clean run and the hanging run differ by `0x100` — 256 bytes.** Peak depth, overshoot past
`limit` (≈11 KB in both, i.e. the sampled guard's unchecked excursion plus the `MemoryError`
unwind), and `used` are all effectively identical. **Stack depth cannot be what distinguishes them**,
so the previous conclusion — "the 96 KB budget is too generous" — was **wrong**.

### Corollary: do not lower `PY_UEFI_FIRMWARE_STACK_BUDGET`

`import re` runs clean but bottoms out only **6 240 bytes above `limit`**. The 96 KB budget is
**nearly too tight already**, not too generous. Lowering it — the change the previous note
proposed — would have started raising `MemoryError` on shallow, working imports and on Phase 8.
Measuring first is what caught this.

**Why the depths are so large at all:** these are `-b NOOPT` builds. No inlining and no frame
reuse means the import machinery's frames are far bigger than a release build's, so ~90 KB for
`import re` is consistent rather than alarming.

## Root cause: `Py_Exit()` bypasses the detach

Diffing the boot trace of the two `import json` runs is decisive. The non-interactive run prints:

```text
Py_RunMain after pymain_run_python      <-- present
edk2_console_detach_readline enter      <-- present
stop_timer: already off                 <-- present
edk2_console_detach_readline leave      <-- present
Py_FinalizeEx enter ... leave
Py_RunMain after Py_FinalizeEx          <-- present
after Py_BytesMain                      <-- present
after main()                            <-- present
after ShellCEntryLib
```

The interactive run goes **straight from `exit()` to `Py_FinalizeEx enter`**, and every line above
is **absent**:

```text
>>> exit()
Py_FinalizeEx enter
Py_FinalizeEx after wait_for_thread_shutdown
Py_FinalizeEx after _PyAtExit_Call
Py_FinalizeEx after flush_std_files
Py_FinalizeEx skip PyGC_Collect (FULL)
Py_FinalizeEx leave
after ShellCEntryLib                    <-- note: no "after Py_BytesMain", no "after main()"
```

`exit()` raises `SystemExit`, which propagates out of `PyRun_InteractiveOne()` into
`PyErr_Print()` → `_Py_HandleSystemExit()` → **`Py_Exit()`**, and `Py_Exit()` calls
`Py_FinalizeEx()` and then `exit()`. It **never returns through `Py_RunMain()`**. But the only
detach call on the normal route lives there:

```730:731:AppPkg/Applications/Python/Python-3.12.13/PyMod-3.12.13/Modules/main.c
    py312_boot_print_ascii("Py_RunMain after pymain_run_python");
    edk2_console_detach_readline();
```

So on the interactive route the image exits having **never called `CloseProtocol`** for
`gEfiSimpleTextInputExProtocolGuid` on `SystemTable->ConsoleInHandle`, and never cancelled the
getkeys timer. The open interface stays registered against an image handle that is about to
disappear, and BDS trips over it when the Shell finally exits.

## Why this matches every observation

| Observation | Explanation |
|-------------|-------------|
| `-S -c` clean, interactive hangs | `-c` never reads interactive input, so **ConInEx is never opened** — nothing to leak, detach or no detach |
| REPL `exit()` returns to the Shell fine | The leak is a firmware protocol registration; Python itself is undamaged, exactly as with the stack bug |
| Hang only at Shell `exit` into BDS | That is when the stale interface is finally touched |
| Depths identical between clean and hanging runs | Correct — depth was never the variable on this path |
| Teardown ladder looks flawless | `Py_FinalizeEx` really does complete; the missing work is *before* it and *after* it |

## Fix

Detach from inside `Py_FinalizeEx()` instead of relying on the `Py_RunMain()` route, so every exit
path is covered — normal return, `Py_Exit()`, and the re-entry cleanup in `Programs/python.c`.
`edk2_console_detach_readline()` is idempotent (it guards on `console_in != NULL`, and
`stop_timer` already reports *"already off"* when there is no timer), so the extra call on the
normal route is harmless.

| File | Change |
|------|--------|
| `PyMod-3.12.13/Python/pylifecycle.c` | `#include "efi/edk2console_api.h"` under `UEFI_C_SOURCE`, and call `edk2_console_detach_readline()` at the `Py_FinalizeEx enter` trace point |

Both `Python312.inf:67` and `Python312_MIN.inf:67` already compile `efi/src/edk2console.c`, and
`PyMod-3.12.13/efi/Include` is on the include path for **both** the MSFT (`:1073`) and GCC
(`:1074`) `CC_FLAGS`, so no INF change is needed and the MIN build still links.

## Verification

Rebuild, then on hardware:

```text
Python312.efi                     -> >>> import json   (MemoryError expected)
                                  -> >>> exit()
                                  -> Shell> exit
```

Expected in the trace, which was **absent** before the fix:

```text
Py_FinalizeEx enter
edk2_console_detach_readline enter
stop_timer: ...
edk2_console_detach_readline leave
```

and **Shell `exit` reaches BIOS setup with no hang**.

Then re-check the routes that already worked, to prove the extra call is harmless — `import re`,
the Phase 8 sweep, and pyreadline phases 2/3 (which should raise `MemoryError`, since
`pyreadline/logger.py` pulls `logging`).

## Still open after this

The **capability** gap is untouched: VS2022 runs on the ~128 KB firmware stack, so `json`,
`logging` and pyreadline remain unusable there and will keep raising `MemoryError`. That needs the
stack switch to work under MSVC — cheapest experiment is to switch the stack but **skip
`py_install_idt()`**, since `edk2_switch_stack()` and `py_install_idt()` are independent calls that
the existing comments blame as a pair.
