# Lab: unified FULL (VS2022 + GCC) after PyMod consolidation (2026-09-04)

**Branch:** `feature/python-3.12.13-vs2022`
**Last code-bearing commit in both images:** **`3afa03f5`** — commits after it (`03023495`, `29eac57b`, `b60db389`) are **docs-only**, so both toolchains ran the **same code state**.
**REPL mode:** default stdio — **`PY_UEFI_READLINE` not set** on either.

| | **VS2022 FULL** | **GCC FULL** |
|--|-----------------|--------------|
| **Clone / tip at build** | `edk2-libc-jp-vsfix` (Windows) @ **`b60db389`** | `/home/jp/src/edk2-libc-jp-vsfix` (WSL) @ **`b60db389`** |
| **Build** | `-t VS2022 -a X64 -b NOOPT` | `-t GCC -a X64 -b NOOPT` |
| **Flags** | `-D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE` | same |
| **Module dir (FULL proof)** | `…\NOOPT_VS2022\X64\…\Python312\DEBUG\` | `…/NOOPT_GCC/X64/…/Python312/DEBUG/` |
| **Image size** | **13,590,016 bytes** | **13,148,160 bytes** |
| **Built** | 12:02:54 | 12:30 |
| **Package** | `myUEFIPy312_latest\EFI\` | `/home/jp/py312_efi/EFI/` (12:32) |
| **Entry path** | `PY_UEFI_MSVC_368_ENTRY` (`ShellCEntryLib`, Shell stack) | `edk2_switch_stack` + `py_install_idt` |

**Image identity verified by hash, not just timestamp** — each deployed `.efi` is byte-identical
to its build output:

| Toolchain | Staged == build | SHA-256 (first 16) |
|-----------|-----------------|--------------------|
| VS2022 FULL | **yes** | — (full-file compare) |
| GCC FULL | **yes** | **`b1a42a9cbd3cab05`** |

Both came from the **`Python312\`** module directory — **not** `Python312_MIN\`. The Windows
MIN image (8.17 MB, `RELEASE_VS2022`, 11:28) was not what shipped.

The GCC image is provably a **new** build, not a re-test of the old one: the 2026-09-01
sign-off image (`/home/jp/myUEFIPy312_GCC_FULL/`) is **13,144,064** bytes /
**`85ae08b27b49ed44`**, versus **13,148,160** / **`b1a42a9cbd3cab05`** here.

WSL clone verified to contain **`9d465ec2`**, **`55219522`**, **`0a674ac0`**, **`3afa03f5`**,
with **24** `frozen_modules/*.h` and `deepfreeze/deepfreeze.c` present.

---

## Why this run matters

Prior sign-offs on this branch predate the PyMod work:

| Toolchain | Prior sign-off | Commit | Predates |
|-----------|----------------|--------|----------|
| VS2022 FULL | 2026-08-26 / 27 | **`3568d02d`** | all of the below |
| GCC FULL | 2026-09-01 | **`dbc8416c`** (11:03) | all of the below |

| Commit | Change |
|--------|--------|
| **`9d465ec2`** (09-03) | Consolidate the 3.12 UEFI port under **`PyMod-3.12.13`** |
| **`55219522`** (09-04) | Track `frozen_modules/*.h` under PyMod for clone-and-build |
| **`0a674ac0`** (09-04) | Fix `fix_deepfreeze_*.py` path resolution (`parents[1]` → `parents[2]`) |
| **`3afa03f5`** (09-04) | Untrack srcprep-generated deepfreeze helpers |

So this is the **first hardware run on either toolchain after the PyMod consolidation**, and
the first time both toolchains are confirmed at the **same** post-consolidation code state.
Both images were confirmed distinct from the prior sign-off builds (see hash table above).

---

## Executed — pass on both

Protocol: each one-liner → **`Shell>`** → **`exit`** → BIOS/setup.

| Command | VS2022 | GCC |
|---------|--------|-----|
| `Python312.efi -S -c "import sys; print(sys.version)"` | **OK** (3.12.13) | **OK** |
| `Python312.efi -S -c "import zlib, ctypes, hashlib; print('ok')"` | **OK** | **OK** |
| `Python312.efi -S -c "import ssl; ssl.create_default_context(); print('ok')"` | **OK** | **OK** |
| `Python312.efi -S -c "import ctypes; print(ctypes.sizeof(ctypes.c_void_p))"` | **`8`** | **`8`** |
| `Python312.efi -S -c "import zlib, ssl, ctypes, hashlib; print('phase8 ok')"` | **`phase8 ok`** | **`phase8 ok`** |
| Shell **`exit`** → firmware after each (hang test) | **No hang** | **No hang** |
| Relaunch without reboot | **OK** — banner normal | **OK** |
| Optional pyreadline | **Not exercised** | **Not exercised** this round (passed 2026-09-01) |

**Two results carry specific weight beyond "no error":**

**`ctypes.sizeof(c_void_p)` → `8` on both.** This is the LLP64 / **`UEFI_MSVC_64`** canary. A
`4` here would mean `libffi_msvc/types.c` built `ffi_type_pointer` as 4 bytes against Python's
8-byte `struct.calcsize("P")`, which surfaces as
**`SystemError: sizeof(py_object) wrong: 4 instead of 8`** (deviations §2.2). `import ctypes`
succeeding does **not** test this; only the value does. Notably it is green on **VS2022**,
where the LLP64 model makes it the real risk — GCC is LP64 and less exposed.

**`phase8 ok` — all four vendored modules in a single process.** Earlier rows imported `ssl`
alone. Loading `zlib`, `ssl`, `ctypes` and `hashlib` together, then tearing down cleanly,
covers the `socket.py` / `selectors` interaction that historically broke Shell `exit` on
VS2022.

**`ssl.create_default_context()` is the load-bearing pass.** It is the primary VS2022 canary
(OpenSSL RNG / NASM ABI, deviations §11.7) and the historical Shell-`exit` hang case. Passing
on both toolchains at `3afa03f5`, with clean teardown and relaunch, means the relocated
frozen/deepfreeze artifacts and the shared PyMod `ssl` package are sound on **both** entry
paths — MSVC 368 and GCC stack-switch + IDT.

---

## Not exercised this round

Applies to **both** toolchains. See [`../Python312_Smoke_Tests.md`](../Python312_Smoke_Tests.md).

The two gaps originally recorded here — **`ctypes.sizeof(c_void_p)`** and the single-process
four-module import — were **closed the same day** and are in the pass table above. Remaining:

| Gap | Note |
|-----|------|
| `ssl.__file__` → UEFI `ssl/__init__.py` | Package-identity check; weaker than the `create_default_context()` pass already obtained |
| `hashlib.sha256(b'x').hexdigest()[:8]` | Value check — only `import hashlib` was exercised |
| `-h`, `print(1+1)`, `import os, sys, json` | §2 baseline rows |
| Interactive `>>>` lines, `exit(0)`, stub `import readline` | Relaunch banner confirmed; individual §4 REPL rows not itemized |
| pyreadline stub-vs-real assertions (§5.2), non-interactive opt-in (§5.3) | New checks; never run on either toolchain |

None of the remaining items guard a known historical failure, unlike the two that were closed.

---

## Status

**VS2022 FULL and GCC FULL @ `3afa03f5`: Phase 8 + LLP64 pointer width + single-process
four-module import + teardown + relaunch — pass on both.**

With `ctypes.sizeof(c_void_p)` and `phase8 ok` green, every check that guards a **known
historical failure** on this port is now covered at this code state:

| Historical failure | Guarding check | Status |
|--------------------|----------------|--------|
| OpenSSL RNG / NASM ABI hang (VS2022) | `ssl.create_default_context()` | **Pass** both |
| `socket.py` / `selectors` Shell `exit` hang | `zlib, ssl, ctypes, hashlib` one process | **Pass** both |
| LLP64 `ffi_type_pointer` 4-vs-8 (VS2022) | `ctypes.sizeof(c_void_p)` == `8` | **Pass** both |
| deepfreeze latin1 / `statically_allocated` | `import sys; sys.version` under `Py_DEBUG` | **Pass** both |
| Interpreter finalize / re-entry | Shell `exit` + relaunch | **Pass** both |

Still not a full re-run of the smoke matrix — §2 baseline rows, the itemised §4 REPL rows and
all pyreadline checks remain open, and GCC pyreadline sign-off still rests on 2026-09-01. But
this run's contribution is **unified regression coverage across the PyMod consolidation on a
single code state**, which the 2026-09-01 unified tag does not cover.

**Candidate tag:** the `ctypes.sizeof` precondition is now met, so a post-PyMod unified pin
can supersede `python312-unified-full-lab-2026-09-01` for clone-and-build. Not yet cut.
