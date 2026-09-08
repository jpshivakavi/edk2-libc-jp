# VS2022 UEFI lab notes (Python 3.12.13)

Ad-hoc **hardware/lab outcomes** and debug playbooks. Canonical build and architecture docs stay in the parent folder:

| Document | Use for |
|----------|---------|
| [`Python312_VS2022_Migration_Status.md`](../Python312_VS2022_Migration_Status.md) | Phase status, session log, branch/workflow |
| [`Python312_VS2022_UEFI_Runtime_Notes.md`](../Python312_VS2022_UEFI_Runtime_Notes.md) | REPL, console, boot trace, teardown |
| [`Python312_VS2022_GCC_Toolchain_Deviations.md`](../Python312_VS2022_GCC_Toolchain_Deviations.md) | GCC vs MSFT splits, §11.6 ssl/Shell |
| [`Python-3.12.13/PyMod-3.12.13/README.txt`](../Python-3.12.13/PyMod-3.12.13/README.txt) | PyMod source of truth, `srcprep.py` |
| [`Python312_Windows_VS2022_Build_Guide.md`](../Python312_Windows_VS2022_Build_Guide.md) | Windows build and package |

## Lab reports

| Date | Report |
|------|--------|
| 2026-08-26 | [`2026-08-26_VS2022_FULL_ssl_Shell_exit.md`](./2026-08-26_VS2022_FULL_ssl_Shell_exit.md) — FULL **`import ssl`** Shell **`exit`** fix + manufacturing smoke |
| 2026-08-27 | [`2026-08-27_VS2022_FULL_ssl_create_default_context_RNG.md`](./2026-08-27_VS2022_FULL_ssl_create_default_context_RNG.md) — **`create_default_context()`** + **`rand_rdrand.nasm`** win64 / **`LNK2001`** |
| 2026-09-01 | [`2026-09-01_GCC_FULL_vs2022_branch_regression.md`](./2026-09-01_GCC_FULL_vs2022_branch_regression.md) — **GCC FULL** on **`feature/python-3.12.13-vs2022`** @ **`dbc8416c`** (Phase 8 matrix + Shell **`exit`**) |
| 2026-09-04 | [`2026-09-04_unified_FULL_post_pymod_smoke.md`](./2026-09-04_unified_FULL_post_pymod_smoke.md) — **VS2022 + GCC FULL** @ **`3afa03f5`** (same code state): first hardware run **after PyMod consolidation**; Phase 8 + **`ssl.create_default_context()`** + **`ctypes.sizeof(c_void_p)`==8** + **`phase8 ok`** + Shell **`exit`** + relaunch on both — all known-failure guards green |

| 2026-09-07 | [`2026-09-07_GCC_FULL_pyreadline_phases.md`](./2026-09-07_GCC_FULL_pyreadline_phases.md) — **GCC FULL** @ **`3afa03f5`**: pyreadline **phases 1–5** all pass; first hardware run of the **stub-vs-real assertions** on any toolchain; moves GCC pyreadline sign-off onto the **post-PyMod pinned state** |
| 2026-09-07 | [`2026-09-07_VS2022_FULL_pyreadline_hang.md`](./2026-09-07_VS2022_FULL_pyreadline_hang.md) — **VS2022 FULL** @ **`3afa03f5`**: pyreadline Shell **`exit`** hang **reproduces** (phases 2/3); stub phases 1/5 clean. **Narrowed: `import readline` alone triggers it — no REPL needed.** Boot-trace diagnostic already compiled in |

| 2026-09-08 | [`2026-09-08_VS2022_FULL_stackcheck_fix.md`](./2026-09-08_VS2022_FULL_stackcheck_fix.md) — **VS2022 FULL** @ **`c3819602`**: **root cause of the Shell `exit` hang found and the safety fix validated.** VS2022 runs Python on the **~128 KB firmware stack** (`PY_UEFI_MSVC_368_ENTRY` returns before the stack switch) while GCC gets **64 MB**, and **`PyOS_CheckStack()` always answered "fine"** because `g_edk2_globals.stack` was NULL on that path. Deep imports now raise **`MemoryError: stack overflow`** and **Shell `exit` is clean on `-S -c` runs**. **PARTIAL:** the **interactive REPL** still hangs; the note's "budget too generous" theory was later disproved. Not a capability fix either: `json`/`logging` still need the stack switch |
| 2026-09-08 | [`2026-09-08_VS2022_nasm_abi_mismatch.md`](./2026-09-08_VS2022_nasm_abi_mismatch.md) — **ROOT ENABLER FOUND.** `edk2_switch_stack`, `edk2_get_idtr` and `edk2_set_idtr` read their arguments from **`rdi`/`rsi`** (System V) while **MSVC passes them in `rcx`/`rdx`** — so the VS2022 stack switch always set `rsp` from garbage. That is the "hangs inside `ShellCEntryLib`" failure `PY_UEFI_MSVC_368_ENTRY` was built to dodge, and the reason VS2022 runs on the ~128 KB firmware stack instead of 64 MB. Fixed with ABI-aware NASM behind `MSFT:*_*_*_NASM_FLAGS`; GCC untouched. **A second defect followed:** the switch moves `rsp` under a live `UefiMain`, and MSVC has no frame pointer, so `image`/`systab` had to be read from globals. **FIXED AND SIGNED OFF** — `json`, `logging`, the REPL repro, Phase 8 and pyreadline §5.3/§5.4 all clean, none raising `MemoryError`. Now the **default** for MSVC; `PY_UEFI_MSVC_368_ENTRY`, `PY_UEFI_MSVC_STACK_SWITCH` and `PY_UEFI_FIRMWARE_STACK_BUDGET` all removed. **MIN still untested on this path** |
| 2026-09-08 | [`2026-09-08_VS2022_FULL_interactive_exit_leak.md`](./2026-09-08_VS2022_FULL_interactive_exit_leak.md) — **VS2022 FULL** @ **`4cf5698a`**: **second, independent root cause.** `stack_min_rsp` instrumentation showed the clean and hanging runs bottom out **256 bytes apart**, so depth was never the variable — and `import re` clears `limit` by only **6 240 B**, meaning the 96 KB budget is nearly too *tight*. Trace diff shows REPL `exit()` routes via **`Py_Exit()`** and never returns through `Py_RunMain()`/`main()`. **Still open:** the leaked-ConInEx mechanism is ruled out (`PY_UEFI_PYREADLINE` is undefined in every INF, so `console_in` is NULL for a stdio REPL). Route vs interactive-stdin is unconfounded by `-c "import sys; sys.exit(0)"` |

Add new dated `YYYY-MM-DD_*.md` files here after lab sessions; link them from this table.
