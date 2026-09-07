# Lab: GCC FULL pyreadline phases 1–5 (2026-09-07)

**Branch:** `feature/python-3.12.13-vs2022`
**Code state:** **`3afa03f5`** — unchanged since 2026-09-04; all 11 commits since are docs-only,
so this is the **same code state pinned by `python312-unified-full-lab-2026-09-04`**.
**Toolchain:** **GCC FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`
**Procedure:** [`../Python312_Smoke_Tests.md`](../Python312_Smoke_Tests.md) **§5.0** phases 1–5
**Result:** **all phases pass — outputs matched the documented expectations at every phase.**

---

## Why this run matters

Two things were open before it, and both are now closed **on GCC**:

**1. The §5.2 / §5.3 assertions had never been run on any toolchain.** Earlier pyreadline
sign-offs confirmed that line editing worked and that a stub `import readline` did not break
teardown, but nothing ever asserted **which code path had actually loaded**. The
`type(readline.rl).__name__` and `sys.modules` discriminators were derived by reading
`readline.py` and had no hardware confirmation. They now do.

**2. GCC pyreadline sign-off was pinned to pre-PyMod code.** It rested on **2026-09-01** @
**`dbc8416c`**, which predates the PyMod-3.12.13 consolidation (**`9d465ec2`**). It now rests
on **`3afa03f5`**, the current pinned state — so the relocated `readline.py` / `pyreadline/`
staging is confirmed good, not merely assumed from the pre-consolidation run.

---

## Phases executed

| Phase | Content | Section | Result |
|------:|---------|---------|--------|
| **1** | Stub is the default (env unset) | §5.2 | **Pass** — `_ReadlineStub`, `False False`, `True None` |
| **2** | Non-interactive opt-in | §5.3 | **Pass** — `Readline True`; Shell `exit` → firmware; relaunch clean |
| **3** | Interactive: `import readline`, history, Tab | §5.4 | **Pass** — recall and completion; `exit()` → Shell → `exit` → firmware |
| **4** | Documented non-bugs | §5.5, §5.6 | **Pass** — behaved as documented |
| **5** | Clear env, re-confirm stub | §5.6 | **Pass** — `set -d`, back to `_ReadlineStub` |

Phase 0 pre-flight (FULL image, `readline.py` and `pyreadline/` staged, env unset) was
satisfied; both packages were confirmed to stage 38 pyreadline files including
`console/edk2.py`.

---

## What each phase actually established

**Phase 1 — the stub is real, not just harmless.** `type(readline.rl).__name__` returning
**`_ReadlineStub`**, with **neither** `pyreadline` **nor** `edk2console` in `sys.modules`,
proves the manufacturing default genuinely does not touch the console. Previous runs could only
show that nothing broke.

**Phase 2 — hook install and teardown both work without a keyboard.** Importing the real
module runs `console.install_readline(rl.readline)` and `rl.read_history_file()`, so a single
`-S -c` covers install plus the `edk2_console_detach_readline` path. This is the cheapest
regression guard for the pyreadline teardown and needs no interactive session.

**Phase 3 — the full operator workflow.** History recall and Tab completion via `edk2console`,
then clean teardown and relaunch.

**Phase 4 — the two traps are hardware-confirmed, not just source-derived.** Both were found by
reading `readline.py` and `Set.c` rather than observed, and now match on hardware:

| Trap | Behaviour |
|------|-----------|
| Value parsing is exact-match | Only `1`, `yes`, `true`, `YES`, `TRUE` enable it; `True` falls through to **stub silently** |
| Arrow keys before `import readline` | `SyntaxError: invalid non-printable character U+001B` — ESC of the escape sequence, not a defect |

**Phase 5 — cleanup verified.** `set -d` followed by a re-assertion of `_ReadlineStub`, which
matters because plain `set` is **non-volatile** and would otherwise persist across reboot and
invalidate later default-mode runs.

---

## Status after this run

| Scenario | GCC FULL | VS2022 FULL |
|----------|----------|-------------|
| Stub default asserted (§5.2) | **Pass** @ `3afa03f5` | **Not run** |
| Non-interactive opt-in (§5.3) | **Pass** @ `3afa03f5` | **Not run** — historically the hang case |
| Interactive history / Tab (§5.4) | **Pass** @ `3afa03f5` | **Not re-labbed** since Session 10 |
| Documented non-bugs (§5.5/§5.6) | **Pass** | n/a |

**GCC:** pyreadline is now fully exercised at the pinned code state. This supersedes, for GCC,
the "pyreadline not covered" caveat in the
`python312-unified-full-lab-2026-09-04` tag message — the tag text is immutable, so this note is
the correction of record.

**VS2022:** unchanged and still **not** signed off for pyreadline. Manufacturing stays stdio.
§5.3 remains the cheapest next step there, and it is the path that historically hung Shell
`exit` — expect to need a power-cycle, and use **`set -v`** so the variable clears on reset.
