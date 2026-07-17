# Phase 8.4 — Enable `ssl` (vendored OpenSSL libssl)

After **8.3** `_hashlib` on the same **`PyMod-3.12.13/Modules/openssl/`** tree.

| Piece | Location |
|-------|----------|
| OpenSSL libssl | **`PyMod-3.12.13/Modules/openssl/ssl/`** (~44 `.c` from `LibOpenSSL.inf`) |
| `_ssl` module | Stock **`Modules/_ssl.c`** (UEFI `OPENSSL_THREADS` guard from edk2-cpython) |
| INF | `Python312.inf` libssl `[Sources]` + `Modules/_ssl.c` |
| Built-ins | `PyMod-3.12.13/Modules/config.c` — `{"_ssl", PyInit__ssl}` |

**Standard reference:** **`~/src/edk2-py312/edk2-openssl`** — `efi/LibOpenSSL/LibOpenSSL.inf` `../../ssl/*` list.

**No** extra `PACKAGES_PATH` packages.

Regenerate INF fragment:

```bash
python3 AppPkg/Applications/Python/tools/gen_openssl_libssl_sources.py > /tmp/openssl_ssl_sources.txt
python3 AppPkg/Applications/Python/tools/patch_python312_inf_ssl.py
```

Refresh `ssl/` tree: `rsync` from edk2-openssl repo `ssl/` into `Modules/openssl/ssl/`.

---

## Build / smoke

Same as [Phase 8.3](./Python312_Phase8_8.3_Hashlib.md) / [WSL GCC guide](./Python312_WSL_GCC_Build_Guide.md):

```bash
build -a X64 -b NOOPT -t GCC -p $EDK2_LIBC_PATH/AppPkg/AppPkg.dsc -D BUILD_PYTHON312
./create_python_pkg.sh GCC NOOPT X64 ~/py312_efi
```

```python
import ssl
ssl.OPENSSL_VERSION_INFO
ctx = ssl.create_default_context()
```

Full TLS to the network may depend on firmware socket/cert policy; minimum smoke is **import** and **default context** construction.

---

## Depends on 8.3

Keep **`e_os.h`**, **`rand_rdrand.nasm`**, and libcrypto `[Sources]` from 8.3; 8.4 only adds **`ssl/`** sources and **`_ssl.c`**.
