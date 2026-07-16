# Phase 8.1 — Enable `zlib` (PyMod vendored layout)

First vendored-library batch per [`Python312_AppPkg_Migration_Plan.md`](./Python312_AppPkg_Migration_Plan.md).

**Layout (same method as Python 3.6.8 AppPkg):**

| Piece | Location |
|-------|----------|
| `zlibmodule.c` | Stock `Python-3.12.13/Modules/zlibmodule.c` |
| zlib 1.2.11 C library | **`PyMod-3.12.13/Modules/zlib/`** (`.c` + headers) |
| INF `[Sources]` | `PyMod-$(PYTHON_VERSION)/Modules/zlib/*.c` (see `Python368.inf` `Modules/zlib` block) |
| Include path | `-I.../PyMod-3.12.13/Modules/zlib` in `Python312.inf` `[BuildOptions]` |

**Source pin:** [edk2-zlib](https://github.com/intel-sandbox/firmware.boot.uefi.python38.edk2-zlib) commit **`8ae7f507f4bce349533b2d231feb8bf1e4e69859`**.  
**No** `LibZlib.dec` / extra `PACKAGES_PATH` segment.

Details: `PyMod-3.12.13/Modules/zlib/README.txt`

---

## Build / smoke

`PACKAGES_PATH=<edk2>:<edk2-libc>` only. StdLib patches + `srcprep.py`, then:

```bash
build -a X64 -b NOOPT -t GCC -p $EDK2_LIBC_PATH/AppPkg/AppPkg.dsc -D BUILD_PYTHON312
```

```python
import zlib
zlib.crc32(b"uefi")
```

---

## Refresh vendored files

Copy the 15 `.c` files from edk2-zlib root (per `efi/LibZlib/LibZlib.inf`) plus required
headers into `PyMod-3.12.13/Modules/zlib/`.
