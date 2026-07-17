#!/usr/bin/env python3
"""Insert libcrypto [Sources] into Python312.inf (Phase 8.3)."""
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
INF = REPO / "Python-3.12.13" / "Python312.inf"
FRAG = Path("/tmp/openssl_sources.txt")

marker_start = "# Phase 8.3–8.4 (later): PyMod-$(PYTHON_VERSION)/Modules/_hashopenssl.c + openssl vendor"
marker_end = "#  PyMod-$(PYTHON_VERSION)/Modules/_ssl.c"

text = INF.read_text()
if "Modules/_hashopenssl.c" in text and "Phase 8.3 — vendored" in text:
    print("already patched")
    raise SystemExit(0)

frag = FRAG.read_text()
replacement = f"""# Phase 8.3 — vendored edk2-openssl libcrypto + _hashlib (_hashopenssl.c)
  Modules/_hashopenssl.c

{frag.rstrip()}

# Phase 8.4 (later): libssl + _ssl.c
#  PyMod-$(PYTHON_VERSION)/Modules/_ssl.c
"""

if marker_start not in text:
    raise SystemExit(f"marker not found in {INF}")

start = text.index(marker_start)
end = text.index(marker_end) + len(marker_end)
new_text = text[:start] + replacement + text[end:]

# Add OPENSSL defines if missing
define_line = '  DEFINE OPENSSL_ROOT       = $(EDK2_LIBC_PATH)/AppPkg/Applications/Python/Python-3.12.13/PyMod-3.12.13/Modules/openssl'
if "DEFINE OPENSSL_ROOT" not in new_text:
    new_text = new_text.replace(
        "  DEFINE LIBFFI_INT_INC",
        define_line + "\n  DEFINE LIBFFI_INT_INC",
        1,
    )

cc_old = "-I$(LIBFFI_INC) -I$(LIBFFI_INT_INC) -DNO_MSABI_VA_FUNCS"
cc_new = (
    "-I$(LIBFFI_INC) -I$(LIBFFI_INT_INC) "
    "-I$(OPENSSL_ROOT)/efi/include -I$(OPENSSL_ROOT) -I$(OPENSSL_ROOT)/include "
    "-DNO_MSABI_VA_FUNCS -Wno-error=maybe-uninitialized -Wno-error=unused-but-set-variable"
)
if cc_new not in new_text:
    new_text = new_text.replace(cc_old, cc_new, 1)

INF.write_text(new_text)
print(f"updated {INF}")
