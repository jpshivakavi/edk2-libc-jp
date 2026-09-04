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

Add new dated `YYYY-MM-DD_*.md` files here after lab sessions; link them from this table.
