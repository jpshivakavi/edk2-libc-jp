# Phase 8.2 — Enable `readline` (vendored edk2-pyreadline)

Per [`Python312_AppPkg_Migration_Plan.md`](./Python312_AppPkg_Migration_Plan.md).

**UEFI is not GNU readline.** The reference port uses **intel-sandbox edk2-pyreadline**
(pure Python **pyreadline** + top-level **readline.py**) with the existing built-in C module
**edk2console** (`PyMod-3.12.13/efi/src/edk2console.c`, already in `Python312.inf`).

**Source pin:** [edk2-pyreadline](https://github.com/intel-sandbox/firmware.boot.uefi.python38.edk2-pyreadline)
commit **`1e9facede991aef35011afdb9c0ff0479fdebab9`**.

**In-tree copy:** `PyMod-3.12.13/Modules/readline/` (`readline.py`, `pyreadline/`, `README.txt`, `COPYING`).

**No** `Modules/readline.c` in the INF. **No** `PACKAGES_PATH` segment.

---

## Packaging

After build, `create_python_pkg.sh` (or `.bat`) copies:

- `Modules/readline/readline.py` → `EFI/lib/python3.12/readline.py`
- `Modules/readline/pyreadline/` → `EFI/lib/python3.12/pyreadline/`

Rebuild the **EFI package** after pulling 8.2; a plain `Python312.efi` swap alone is not enough.

---

## Build (unchanged from 8.1)

```bash
export PACKAGES_PATH=$HOME/src/edk2:$HOME/src/edk2-libc
# patches, srcprep, frozen, then:
build -a X64 -b NOOPT -t GCC -p $EDK2_LIBC_PATH/AppPkg/AppPkg.dsc -D BUILD_PYTHON312
./create_python_pkg.sh GCC NOOPT X64 ~/py312_efi
```

---

## Smoke

```python
import readline
readline.__doc__
# Interactive REPL: tab completion, line editing (via edk2console)
```

`site.py` registers `sys.__interactivehook__` when `readline` imports successfully.
