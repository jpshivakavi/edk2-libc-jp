#!/usr/bin/env python3
"""Insert libssl [Sources] + _ssl.c into Python312.inf (Phase 8.4)."""
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
INF = REPO / "Python-3.12.13" / "Python312.inf"
FRAG = Path("/tmp/openssl_ssl_sources.txt")

marker = "# Phase 8.4 (later): libssl + _ssl.c"

text = INF.read_text()
if "Modules/_ssl.c" in text and "Phase 8.4 — libssl" in text:
    print("already patched")
    raise SystemExit(0)

if marker not in text:
    raise SystemExit(f"marker not found in {INF}")

frag = FRAG.read_text()
replacement = f"""# Phase 8.4 — libssl + _ssl (same vendored OpenSSL tree as 8.3)
  Modules/_ssl.c

{frag.rstrip()}
"""

start = text.index(marker)
end = start + len(marker)
# drop commented _ssl.c line if present
rest = text[end:]
if rest.lstrip().startswith("#  PyMod-$(PYTHON_VERSION)/Modules/_ssl.c"):
    end = end + rest.index("\n", rest.find("_ssl.c")) + 1

new_text = text[:start] + replacement + text[end:]
INF.write_text(new_text)
print(f"updated {INF}")
