# Phase 8.3 — Enable `_hashlib` / `hashlib` (vendored OpenSSL libcrypto)

After **8.1** zlib, **8.2** readline, and **8.5** libffi on this fork. **Before** **8.4** `_ssl`.

| Piece | Location |
|-------|----------|
| OpenSSL 1.1.1f libcrypto | **`PyMod-3.12.13/Modules/openssl/`** |
| `_hashlib` module | Stock **`Modules/_hashopenssl.c`** (UEFI guard from edk2-cpython) |
| INF | `Python312.inf` libcrypto `[Sources]` + `_hashopenssl.c` |
| Built-ins | `PyMod-3.12.13/Modules/config.c` — `{"_hashlib", PyInit__hashlib}` |

**Standard reference:** **`~/src/edk2-py312/edk2-openssl`** — `efi/LibOpenSSL/LibOpenSSL.inf`
(commit **`59db29b`**). Phase 8.3 uses the same libcrypto file list **minus** `ssl/` and `*.nasm`
(8.4 adds SSL).

**No** `LibOpenSSL.dec` / extra `PACKAGES_PATH` segment.

Details: `PyMod-3.12.13/Modules/openssl/README.txt`

Regenerate INF fragment:

```bash
python3 AppPkg/Applications/Python/tools/gen_openssl_libcrypto_sources.py
python3 AppPkg/Applications/Python/tools/patch_python312_inf_openssl.py
```

---

## Build / smoke

`PACKAGES_PATH=<edk2>:<edk2-libc>` only. StdLib patches + `srcprep.py`, then:

```bash
build -a X64 -b NOOPT -t GCC -p $EDK2_LIBC_PATH/AppPkg/AppPkg.dsc -D BUILD_PYTHON312
./create_python_pkg.sh GCC NOOPT X64 ~/py312_efi
```

```python
import hashlib
hashlib.sha256(b"x").hexdigest()
# expect: 2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae
```

Pure-Python / HACL `_sha*` modules still work without OpenSSL; this step enables the
OpenSSL-backed `_hashlib` path used by `hashlib` for additional algorithms and `openssl_*` helpers.

---

## Refresh vendored OpenSSL

From **`edk2-py312/edk2-openssl`** (pin commit in `Modules/openssl/README.txt`):

1. `rsync` `crypto/`, `engines/`, `include/`, `efi/` into `PyMod-.../Modules/openssl/`.
2. Re-run `gen_openssl_libcrypto_sources.py` after any `LibOpenSSL.inf` change upstream.
3. Mirror `[BuildOptions]` `-I` paths from `LibOpenSSL.inf` in `Python312.inf`.
