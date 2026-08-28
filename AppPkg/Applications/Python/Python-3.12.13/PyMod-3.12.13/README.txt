PyMod-3.12.13 ? UEFI overlays for Python 3.12.13 (AppPkg)

Source of truth for all UEFI-specific C, headers, and stdlib Python patches.
The stock tree under Python-3.12.13/ should match upstream CPython 3.12.13 for
paths that are forked here; Python312.inf compiles PyMod paths for those .c files.

Before build / packaging:
  python3 srcprep.py
copies PyMod *.h and *.py into the CPython tree (see Py312ReadMe.txt).

EFI application glue lives only under PyMod-3.12.13/efi/ (not Python-3.12.13/efi/).

Phase 8 (FULL): ctypes, ssl, hashlib, zlib, and readline are vendored under
PyMod-3.12.13/Modules/ and enabled in Modules/config.c + Python312.inf.

External / third-party library layout (mirror Python 3.6.8 AppPkg):

  Stock CPython module glue stays under Python-3.12.13/Modules/ when unchanged
  (e.g. Modules/zlibmodule.c). Vendored library C sources and UEFI-adopted module
  trees live under PyMod-3.12.13/Modules/; Python312.inf compiles PyMod paths.

  Phase 8.1 (done):
    Modules/zlib/          edk2-zlib @ 8ae7f507 (INF: PyMod-$(PYTHON_VERSION)/Modules/zlib/*.c)

  Phase 8.2 (done):
    Modules/readline/      edk2-pyreadline @ 1e9face (readline.py + pyreadline/; staged to Lib by create_python_pkg)

  Phase 8.3 (done):
    Modules/openssl/       libcrypto + _hashopenssl.c (see Python312_Phase8_8.3_Hashlib.md)

  Phase 8.4 (done):
    Modules/openssl/ssl/   libssl + _ssl.c (see Python312_Phase8_8.4_Ssl.md)

  Phase 8.5 (done):
    Modules/libffi/        + _ctypes / _ctypes_test (see Python312_Phase8_8.5_Ctypes.md)

Inventory (mirrored path = same relative path under PyMod-3.12.13/):

  Programs/python.c
  Parser/tokenizer.c
  Objects/complexobject.c, Objects/floatobject.c
  Python/bootstrap_hash.c, fileutils.c, getargs.c, pystate.c, pytime.c,
    sysmodule.c, pylifecycle.c
  Modules/config.c, getpath.c, gcmodule.c, posixmodule.c, timemodule.c,
    signalmodule.c, socketmodule.c, faulthandler.c, mathmodule.c, cmathmodule.c,
    _datetimemodule.c, mmapmodule.c, termios.c, _pickle.c, _posixsubprocess.c,
    main.c, _ssl.c,
    expat/xmlparse.c
  Modules/clinic/posixmodule.c.h
  Include/pyconfig.h, pymath.h, dlfcn.h, pthread.h
  Include/internal/pycore_bitutils.h
  Modules/_sre/sre_lib.h
  Modules/_decimal/libmpdec/mpdecimal.h
  Modules/_hacl/include/krml/lowstar_endianness.h
  Modules/zlib/              vendored zlib (edk2-zlib 8ae7f507)
  Modules/readline/          vendored pyreadline (edk2-pyreadline 1e9face)
  Modules/openssl/           vendored OpenSSL 1.1.1f (libcrypto + libssl)
  Modules/libffi/            vendored libffi (_ctypes)
  Lib/os.py, pathlib.py, site.py, uefipath.py, socket.py
  Lib/ssl/               UEFI minimal package (__init__.py, _uefi_min.py, _stdlib.py)
  Lib/importlib/_bootstrap_external.py
  Lib/asyncio/uefi_events.py
  efi/src/py312_openssl_uefi.c
  efi/                     UefiMain, stack/handler NASM, dummies, pyconfig for -I

Do not reintroduce UEFI edits under Python-3.12.13/ for the paths above.
