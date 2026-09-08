"""T9: reproduce PyErr_Print()'s handling of an uncaught stack-overflow
MemoryError outside the REPL, then keep running.

The VS2022 Shell-exit hang needs all three of: the stack guard firing, the
MemoryError being uncaught, and execution continuing afterwards. A plain
try/except is clean because it never prints the traceback and never pins it in
sys.last_*. This does both, so a hang here takes the REPL out of the repro.

Run: Python312.efi -S t9_uncaught_overflow.py
Must be saved as ASCII/UTF-8 with no BOM. The UEFI Shell's `edit` writes UTF-16,
which CPython rejects with "Non-UTF-8 code starting with '\\xff'".
"""
import sys

try:
    import json
except MemoryError:
    info = sys.exc_info()
    sys.last_type, sys.last_value, sys.last_traceback = info
    sys.excepthook(*info)

s = input('t = ')
print('ok', s)
