PyMod-3.12.13 ù UEFI overlays for Python 3.12.13 (AppPkg)

Source of truth for all UEFI-specific C, headers, and stdlib Python patches.
The stock tree under Python-3.12.13/ should match upstream CPython 3.12.13 for
paths that are forked here; Python312.inf compiles PyMod paths for those .c files.

Before build / packaging:
  python3 srcprep.py
copies PyMod *.h and *.py into the CPython tree (see Py312ReadMe.txt).

EFI application glue lives only under PyMod-3.12.13/efi/ (not Python-3.12.13/efi/).

Iteration 1 omitted ctypes/ssl/zlib/readline until Phase 8 vendoring under PyMod/Modules/.

External / third-party library layout (mirror Python 3.6.8 AppPkg):

  Stock CPython module glue stays under Python-3.12.13/Modules/ when unchanged
  (e.g. Modules/zlibmodule.c). Vendored library C sources and UEFI-adopted module
  trees live under PyMod-3.12.13/Modules/; Python312.inf compiles PyMod paths.

  Phase 8.1 (done):
    Modules/zlib/          edk2-zlib @ 8ae7f507 (INF: PyMod-$(PYTHON_VERSION)/Modules/zlib/*.c)

  Phase 8.2 (done):
    Modules/readline/      edk2-pyreadline @ 1e9face (readline.py + pyreadline/; staged to Lib by create_python_pkg)

  Phase 8 planned:
    Modules/openssl/       (or split libcrypto) + _hashopenssl.c, _ssl.c
    Modules/_ctypes/       + libffi vendor tree (see 3.6.8 PyMod _ctypes/libffi_msvc)

Inventory (mirrored path = same relative path under PyMod-3.12.13/):

  Programs/python.c
  Parser/tokenizer.c
  Objects/complexobject.c, Objects/floatobject.c
  Python/bootstrap_hash.c, fileutils.c, getargs.c, pystate.c, pytime.c, sysmodule.c
  Modules/config.c, getpath.c, gcmodule.c, posixmodule.c, timemodule.c,
    signalmodule.c, socketmodule.c, faulthandler.c, mathmodule.c, cmathmodule.c,
    _datetimemodule.c, mmapmodule.c, termios.c, _pickle.c, _posixsubprocess.c,
    expat/xmlparse.c
  Modules/clinic/posixmodule.c.h
  Include/pyconfig.h, pymath.h, dlfcn.h, pthread.h
  Include/internal/pycore_bitutils.h
  Modules/_sre/sre_lib.h
  Modules/_decimal/libmpdec/mpdecimal.h
  Modules/_hacl/include/krml/lowstar_endianness.h
  Modules/zlib/              vendored zlib (edk2-zlib 8ae7f507)
  Modules/readline/          vendored pyreadline (edk2-pyreadline 1e9face)
  Lib/os.py, pathlib.py, site.py, uefipath.py
  Lib/importlib/_bootstrap_external.py
  Lib/asyncio/uefi_events.py
  efi/                     UefiMain, stack/handler NASM, dummies, pyconfig for -I

Do not reintroduce UEFI edits under Python-3.12.13/ for the paths above.
