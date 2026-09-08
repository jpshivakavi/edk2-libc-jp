"""ssl — UEFI uses a minimal wrapper; other platforms use the full stdlib module."""
import os

if os.name == 'uefi':
    from ssl._uefi_min import *  # noqa: F403
else:
    from ssl._stdlib import *  # noqa: F403
