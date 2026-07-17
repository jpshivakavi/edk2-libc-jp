                                EDK II Python
                                   ReadMe
                                Version 3.12.13
                                 AppPkg port
                               FULL (Phase 8, GCC)
                               17 July 2026


1. OVERVIEW
===========
This document describes building, packaging, and running the CPython 3.12.13
interpreter as a UEFI Shell application using the edk2-libc AppPkg layout
(same pattern as Python 3.6.8 in AppPkg/Applications/Python/Python-3.6.8).

Prerequisites:
  * EDK II (tianocore/edk2) with BaseTools built on Linux or WSL.
  * This edk2-libc tree on branch feature/python-3.12.13-apppkg (or equivalent).
  * GCC toolchain, NASM, and host Python 3.x for srcprep / frozen steps.

Detailed migration and status:
  ../Python312_AppPkg_Migration_Plan.md
  ../Python312_AppPkg_Migration_Status.md
  ../Python312_WSL_GCC_Build_Guide.md
  ../Python312_UEFI_Startup_Messages.md   (boot-time Print / REPL banner)
  GCCCompilationBKMs.rst          (GCC build BKMs; companion to 3.6.8 BKMs)

Source alignment (important):
  Sync UEFI CPython deltas from ~/src/edk2-py31213/edk2-cpython (3.12.13).
  Do not bulk-sync from ~/src/edk2-py312/edk2-cpython (that tree was 3.12.0).


2. RELEASE NOTES (FULL — edk2-py312 module parity on fork)
========================================================
  1)  All C extension modules are statically linked into Python312.efi.
  2)  Dynamically loadable extensions (.so / .pyd) are not supported.
  3)  Pure Python stdlib lives under EFI\lib\python3.12\ on the target volume.
  4)  An empty EFI\lib\python3.12\lib-dynload\ directory is required so
      getpath can resolve exec_prefix (no .pyd files are needed).
  5)  PACKAGES_PATH = <edk2>;<edk2-libc> only.  Third-party libs are **vendored**
      under PyMod-3.12.13/Modules/ (zlib, libffi, OpenSSL, pyreadline) — do **not**
      add intel-sandbox edk2-zlib / edk2-openssl / edk2-libffi / edk2-pyreadline
      to PACKAGES_PATH.
  6)  **Phase 8 built-ins (static):** zlib, pyreadline (package + edk2console),
      _ctypes / _ctypes_test, _hashlib (OpenSSL libcrypto), _ssl (OpenSSL libssl).
      OpenSSL on UEFI is **1.1.1f** (edk2-openssl vendor) — ssl.OPENSSL_VERSION_INFO
      will **not** match Windows CPython 3.12.x linked to OpenSSL 3.x.
  7)  User-specific site configuration is limited; see site.py on the target.
  8)  Environment variable support is partial (UEFI environ); PYTHONHOME
      can override prefix if the EFI tree is not on fs0:.

  Milestone tags (fork jpshivakavi/edk2-libc-jp): python312-apppkg-8.2 … 8.5,
  8.3, 8.4 — see ../Python312_AppPkg_Migration_Status.md.


3. GETTING AND BUILDING PYTHON
==============================

  3.1 Tree layout
  ---------------
  AppPkg/Applications/Python/Python-3.12.13/
    Python-3.12.13/          Upstream CPython 3.12.13 sources (no in-tree UEFI forks)
    PyMod-3.12.13/           UEFI overlays (.c/.h/.py); EFI entry (efi/); see README.txt
    Python312.inf            Monolithic EDK II module (core + Phase 8 vendored libs)
    srcprep.py               Copies PyMod .h/.py into the CPython tree
    patches/                 StdLib patches (apply locally; see section 3.2)
    create_python_pkg.sh     Stage EFI runtime tree for FAT / hardware
    create_python_pkg.bat    Windows variant of packaging script

  3.2 Required: apply libc patches (do NOT commit StdLib on the branch)
  ---------------------------------------------------------------------
  Four patches under patches/ are required for link and runtime behavior.
  Keep StdLib/ vanilla in git; apply before each fresh checkout / build:

    cd <edk2-libc>
    git apply --check --ignore-whitespace \
      AppPkg/Applications/Python/Python-3.12.13/patches/*.patch
    git apply --ignore-whitespace \
      AppPkg/Applications/Python/Python-3.12.13/patches/*.patch

    ls StdLib/LibC/Uefi/upipe.c

  Files:
    0001-Implement-minimal-emulation-of-pipe-functionality.patch
    0002-Introduce-support-for-ANSI-escape-codes-for-console.patch
    0003-Fix-uninitialized-static-variable.patch
    0004-Fix-ioctl-vararg-handling-for-Console-and-Shell-devi.patch

  To discard local StdLib dirt and re-apply:
    git checkout -- StdLib StdLibPrivateInternalFiles
    git clean -fd StdLib
    (then git apply again)

  3.3 Prep (headers, overlays, frozen)
  ------------------------------------
    cd AppPkg/Applications/Python/Python-3.12.13
    python3 srcprep.py

  Run srcprep after changing PyMod-3.12.13/*.h or overlay *.py.

  Do not add UEFI_C_SOURCE edits under Python-3.12.13/ for paths listed in
  PyMod-3.12.13/README.txt; Python312.inf builds those .c files from PyMod.

  Frozen / deepfreeze artifacts are gitignored.  Before the first build on a
  clean tree, ensure Python/deepfreeze/deepfreeze.c and related frozen headers
  exist.  Regeneration steps are in Python312_WSL_GCC_Build_Guide.md (use the
  edk2-py31213 CPython tree as the reference revision).

  3.4 Built-in module set (config.c)
  -----------------------------------
  Enabled extensions are listed in PyMod-3.12.13/Modules/config.c
  (_PyImport_Inittab).  Python312.inf lists all linked .c / asm sources.

  Phase 8 vendored (enabled on feature/python-3.12.13-apppkg):
    zlib, readline (pyreadline in EFI lib tree), _ctypes, _ctypes_test,
    _hashlib, _ssl

  Vendor trees: PyMod-3.12.13/Modules/zlib/, libffi/, openssl/, readline/
  Guides: ../Python312_Phase8_8.1_Zlib.md … 8.5, 8.3, 8.4 under AppPkg/Applications/Python/

  3.5 Environment variables (build host)
  --------------------------------------
    export PACKAGES_PATH=<path_to_edk2>:<path_to_edk2-libc>
    export EDK2_LIBC_PATH=<path_to_edk2-libc>
    export WORKSPACE=<path_to_edk2>
    export PYTHON_COMMAND=python3

    cd $WORKSPACE
    . edksetup.sh

  Example WORKSPACE layout used in development:
    WORKSPACE=~/src/edk2-py312/edk2
    EDK2_LIBC_PATH=~/src/edk2-libc
    PACKAGES_PATH=$WORKSPACE:$EDK2_LIBC_PATH

  3.6 Build command
  -----------------
    build -a X64 -b NOOPT -t GCC \
      -p $EDK2_LIBC_PATH/AppPkg/AppPkg.dsc \
      -D BUILD_PYTHON312

  Output (typical):
    Build/AppPkg/NOOPT_GCC/X64/Python312.efi
    or under .../AppPkg/Applications/Python/Python-3.12.13/Python312/DEBUG/

  After changing Include/patchlevel.h or core sources, delete the Python312
  build output directory under Build/AppPkg/.../Python-3.12.13 before
  rebuilding so the EFI embeds the correct version string.


4. PYTHON-RELATED PATHS AND FILES (TARGET)
==========================================
Iteration 1 PREFIX (unchanged for FULL; from PyMod Include/pyconfig.h):

  PLATFORM     uefi
  PREFIX       fs0:\EFI
  EXEC_PREFIX  fs0:\EFI
  stdlib       fs0:\EFI\lib\python3.12
  platstdlib   fs0:\EFI\lib\python3.12\lib-dynload   (empty directory)

Layout produced by create_python_pkg.sh:

  \EFI
    \bin
        Python312.efi          UEFI Shell application (interpreter)
    \lib
        \python3.12            Pure Python from Python-3.12.13/Lib (+ PyMod Lib)
            \lib-dynload       Empty; required for getpath
    \stdlib
        \etc                   hosts, resolv.conf, etc. (from StdLib/Efi/StdLib/etc)

If the package is installed on another filesystem (e.g. fs1:), set PYTHONHOME
to that volume's \EFI prefix before starting Python312.efi.


5. INSTALLING / PACKAGING
=========================
From AppPkg/Applications/Python/Python-3.12.13 (with WORKSPACE and
EDK2_LIBC_PATH set):

  ./create_python_pkg.sh GCC NOOPT X64 /path/to/out

Windows:

  create_python_pkg.bat GCC NOOPT X64 C:\path\to\out

Copies Python312.efi, Lib/, and stdlib etc/ into <out>\EFI\...

Copy the EFI\ tree to a FAT32 UEFI volume.  Map the volume in Shell (e.g. fs0:).


6. EXAMPLE: SOCKET SUPPORT (enabled)
====================================
Python312.inf includes socketmodule.c and links BsdSocketLib / EfiSocketLib.
_socket is registered in config.c.

To disable sockets: remove or comment the socket entries in config.c and
Python312.inf, drop BsdSocketLib from [LibraryClasses], and rebuild.

_ssl uses the same vendored OpenSSL tree as _hashlib (Phase 8.3–8.4); no
separate edk2-openssl on PACKAGES_PATH.


7. RUNNING PYTHON
=================
Run from a FAT partition under the UEFI Shell.

  Shell> fs0:
  FS0:\> cd EFI\bin
  FS0:\EFI\bin> Python312.efi

Expected banner includes 3.12.13 and platform uefi.

REPL smoke (FULL — after create_python_pkg):

  >>> import sys
  >>> print(sys.version)
  >>> print(sys.platform)
  >>> import os, json
  >>> import zlib; import readline; import ctypes; import hashlib; import ssl
  >>> hashlib.sha256(b"x").hexdigest()
  >>> ssl.create_default_context()

Interactive input: pyreadline (staged under EFI\lib\python3.12\) plus
Parser/myreadline.c and edk2console (not GNU readline.so).


8. SUPPORTED C MODULES (FULL)
=============================
Built-in table: PyMod-3.12.13/Modules/config.c

Core / always present:
  atexit, faulthandler, uefi, _signal, _tracemalloc, _codecs, _collections,
  errno, _io, itertools, _sre, _thread, time, _weakref, _abc, _functools,
  _locale, _operator, _stat, _symtable, pwd, marshal, _imp, _ast, _tokenize,
  gc, _warnings, _string

UEFI_C_SOURCE extensions (static, in Python312.efi):
  _struct, array, _contextvars, math, cmath, _elementtree, _datetime, _random,
  _bisect, _heapq, _lsprof, unicodedata, _opcode, _asyncio, _queue, select,
  _xxsubinterpreters, _xxinterpchannels, audioop, _csv, _socket,
  _md5, _sha1, _blake2, _sha3, binascii, _multibytecodec, _decimal,
  xxlimited, _posixsubprocess, edk2console, pyexpat, _typing, _json,
  _multiprocessing, _zoneinfo, _pickle, _statistics, _sha2, mmap, termios,
  zlib, _ctypes, _ctypes_test, _hashlib, _ssl

Pure Python readline: import readline (pyreadline copied by create_python_pkg.*)

Interpreter / frozen (not separate import names):
  Deep-frozen stdlib fragments, getpath, ceval, etc. are inside the EFI image.


9. PURE PYTHON STDLIB
=====================
The create_python_pkg scripts copy AppPkg/.../Python-3.12.13/Lib (and PyMod
Lib overlays when present) to EFI\lib\python3.12\.

Not every CPython 3.12.13 Lib file is validated on UEFI.  Start with modules
you need and extend testing over time.

Suggested smoke imports:
  encodings, json, hashlib, ssl, re, os, pathlib, collections, datetime,
  zlib, ctypes, readline


10. TROUBLESHOOTING
===================
  * Link errors mentioning upipe / pipe: libc patch 0001 not applied.
  * "Could not find platform dependent libraries <exec_prefix>": create
    EFI\lib\python3.12\lib-dynload on the target volume.
  * Banner shows 3.12.0: wrong sync source or stale Build/ objects; use
    edk2-py31213 and rebuild Python312 after wiping its build output.
  * Mixed API / link errors after partial file copies: re-sync whole
    Objects/, Python/, Parser/, Include/, Modules/ from edk2-py31213.

# # #
