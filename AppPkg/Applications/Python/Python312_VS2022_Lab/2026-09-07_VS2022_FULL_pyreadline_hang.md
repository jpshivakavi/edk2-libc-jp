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

## Next diagnostic — instrumentation is already in the image

**`/DPY_UEFI_BOOT_TRACE=1` is already on `MSFT:*_*_*_CC_FLAGS` in `Python312.inf`**, so
`PY312_CONSOLE_TRACE` in `edk2console.c` is **live in the image already tested** — no rebuild
needed. Re-run phase 2 and read the console before typing `exit`:

```text
set -v PY_UEFI_READLINE 1
Python312.efi -S -c "import readline, sys; print(type(readline.rl).__name__)"
```

Relevant lines all start **`Python312 boot:`**. Look for:

```text
stop_timer: TimerCancel   /  stop_timer: CloseEvent  /  stop_timer: done
edk2_console_detach_readline enter
edk2_console_detach_readline leave
edk2_console_handoff_to_shell enter / leave
```

| Trace seen | Interpretation |
|------------|----------------|
| `detach_readline enter` **and** `leave` | Detach completed; the leak is **after** it — ConIn/handle state left for the Shell, or an uncancelled event. Look at `edk2_console_handoff_to_shell` and the timer path |
| `enter` but **no** `leave` | Hang is **inside** detach — the `edk2_console_drain_input()` `while (ReadKeyStrokeEx…)` loop or `CloseProtocol`. But note Python *did* reach `Shell>`, so this is unlikely |
| **neither** | Detach never ran on this path — the `-S -c` exit route misses the call sites in `python.c` / `main.c` |
| `stop_timer` lines absent | The `getkeys` timer event was never created or never cancelled |

**Known dead end — do not retry blind:** `edk2console.c` documents that resetting ConIn does
not help, and both directions were already tried:

> *"Do not Reset ConIn here; `Reset(TRUE)` hangs after REPL exit and `Reset(FALSE)` has hung the
> next Shell launch on VS2022. Drain only."*

So the fix is not a ConIn reset. The trace outcome above should decide the direction before any
code change.

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
