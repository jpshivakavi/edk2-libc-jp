# Lab: VS2022 FULL after PyMod consolidation (2026-09-04)

**Branch:** `feature/python-3.12.13-vs2022` · **Tree tip at build:** **`b60db389`**
**Last code-bearing commit in image:** **`3afa03f5`** (commits after it are docs-only)
**Build:** `build -t VS2022 -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE`
**Image:** FULL — `…\NOOPT_VS2022\X64\…\Python312\OUTPUT\Python312.efi`, **12.96 MB**, built 12:02:54
**Package:** `myUEFIPy312_latest\EFI\` — deployed `.efi` matches the FULL build by size and timestamp (**not** the 8.17 MB `RELEASE_VS2022` `Python312_MIN` image built at 11:28)
**Entry:** `PY_UEFI_MSVC_368_ENTRY` (MSVC 368 path)
**REPL mode:** default stdio — **`PY_UEFI_READLINE` not set**

---

## Why this run matters

Previous VS2022 FULL sign-off was **2026-08-26 / 27** at **`3568d02d`**. That predates:

| Commit | Change |
|--------|--------|
| **`9d465ec2`** | Consolidate the 3.12 UEFI port under **`PyMod-3.12.13`** |
| **`55219522`** | Track `frozen_modules/*.h` under PyMod for clone-and-build |
| **`0a674ac0`** | Fix `fix_deepfreeze_*.py` path resolution (`parents[1]` → `parents[2]`) |
| **`3afa03f5`** | Untrack srcprep-generated deepfreeze helpers |

So this is the **first VS2022 FULL hardware run after the PyMod consolidation and the
deepfreeze helper fixes** — it confirms the relocated frozen/deepfreeze artifacts still
produce a working FULL interpreter on MSVC.

---

## Executed — pass

Protocol: each one-liner → **`Shell>`** → **`exit`** → BIOS/setup.

| Command | Result |
|---------|--------|
| `Python312.efi -S -c "import sys; print(sys.version)"` | **OK** — 3.12.13 |
| `Python312.efi -S -c "import zlib, ctypes, hashlib; print('ok')"` | **OK** |
| `Python312.efi -S -c "import ssl; ssl.create_default_context(); print('ok')"` | **OK** |

| Additional check | Result |
|------------------|--------|
| Shell **`exit`** → firmware after each (hang test) | **No hang** |
| Relaunch `Python312.efi` without reboot | **OK** — banner normal, no `py312_uefi_reentry_cleanup` |
| Optional pyreadline | **Not exercised** — stayed on manufacturing stdio default |

**`ssl.create_default_context()` is the load-bearing pass here.** It is the primary VS2022
canary (OpenSSL RNG / NASM ABI, deviations §11.7) and the historical Shell-`exit` hang case,
and it survived the PyMod move. Combined with the clean teardown and relaunch, the
MSVC finalize/teardown path is intact at `3afa03f5`.

---

## Not exercised this round

Recorded so the next run knows the gap. See [`../Python312_Smoke_Tests.md`](../Python312_Smoke_Tests.md).

| Gap | Note |
|-----|------|
| **`ctypes.sizeof(ctypes.c_void_p)` → `8`** | **Highest-value gap.** `import ctypes` succeeding does **not** prove pointer width; only the `sizeof` value catches the LLP64 / `UEFI_MSVC_64` bug (deviations §2.2) |
| `import zlib, ssl, ctypes, hashlib` in **one** process | `ssl` was imported alone; all four together is a distinct case given the historical `socket.py` / `selectors` teardown chain |
| `ssl.__file__` → UEFI `ssl/__init__.py` | Package-identity check; weaker than the `create_default_context()` pass already obtained |
| `hashlib.sha256(b'x').hexdigest()[:8]` | Value check — only `import hashlib` was exercised |
| `-h`, `print(1+1)`, `import os, sys, json` | §2 baseline rows |
| Interactive `>>>` lines, `exit(0)`, stub `import readline` | Relaunch banner was confirmed; individual §4 REPL rows not itemized |

**Cheapest way to close the main gap:**

```text
Python312.efi -S -c "import ctypes; print(ctypes.sizeof(ctypes.c_void_p))"
Python312.efi -S -c "import zlib, ssl, ctypes, hashlib; print('phase8 ok')"
```

Expect **`8`** and **`phase8 ok`**, each followed by Shell **`exit`** reaching firmware.

---

## Status

**VS2022 FULL @ `3afa03f5`: Phase 8 spot-check + teardown + relaunch — pass.**

Not a full re-run of the smoke matrix; V6 sign-off from 2026-09-01 remains the
comprehensive record. This run's contribution is **regression coverage across the PyMod
consolidation**, not broadened scope.
