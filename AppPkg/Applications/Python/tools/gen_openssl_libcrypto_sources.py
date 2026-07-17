#!/usr/bin/env python3
"""Generate libcrypto [Sources] paths for Python312.inf from edk2-openssl LibOpenSSL.inf."""
import re
import sys
from pathlib import Path

INF = Path(sys.argv[1] if len(sys.argv) > 1 else
           "/home/jp/src/edk2-py312/edk2-openssl/efi/LibOpenSSL/LibOpenSSL.inf")
LIBOPENSSL_DIR = INF.parent

def resolve(rel: str) -> Path:
    return (LIBOPENSSL_DIR / rel).resolve()

def to_pymod_rel(abs_path: Path, openssl_root: Path) -> str:
    """Map edk2-openssl file to PyMod-3.12.13/Modules/openssl/ relative path."""
    try:
        rel = abs_path.relative_to(openssl_root)
    except ValueError:
        raise SystemExit(f"not under openssl root: {abs_path}")
    return "PyMod-3.12.13/Modules/openssl/" + rel.as_posix()

def main():
    openssl_root = LIBOPENSSL_DIR.parent.parent.resolve()
    text = INF.read_text()
    paths = []
    for line in text.splitlines():
        m = re.match(r"\s+(\S+)", line)
        if not m:
            continue
        p = m.group(1)
        if p.startswith("#"):
            continue
        if "/ssl/" in p or p.startswith("../../ssl/"):
            continue
        if p.endswith(".nasm"):
            continue
        if not p.startswith("../"):
            continue
        ap = resolve(p)
        if not ap.is_file():
            print(f"MISSING: {p} -> {ap}", file=sys.stderr)
            continue
        paths.append(to_pymod_rel(ap, openssl_root))

    prefix = "PyMod-$(PYTHON_VERSION)/Modules/openssl"
    print(f"# libcrypto sources ({len(paths)} files) — from LibOpenSSL.inf minus ssl/ and *.nasm")
    print(prefix)
    for p in paths:
        # strip PyMod-3.12.13 -> use DEFINE
        rest = p.replace("PyMod-3.12.13/", "PyMod-$(PYTHON_VERSION)/")
        print(f"  {rest}")

if __name__ == "__main__":
    main()
