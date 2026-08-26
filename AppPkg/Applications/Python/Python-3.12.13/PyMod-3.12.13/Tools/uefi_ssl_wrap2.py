"""Wrap SSLObject/SSLSocket for non-UEFI; add UEFI stubs."""
from pathlib import Path

p = Path(__file__).resolve().parents[1] / "Lib" / "ssl.py"
lines = p.read_text(encoding="utf-8").splitlines(keepends=True)

start = end = None
for i, line in enumerate(lines):
    if line.startswith("class SSLObject:"):
        start = i
    if start is not None and line.startswith("if os.name != 'uefi':") and i > start:
        if i + 1 < len(lines) and "Python does not support" in lines[i + 1]:
            end = i - 1
            break

if start is None or end is None:
    raise SystemExit(f"block not found start={start} end={end}")

if start > 0 and lines[start - 1].strip().startswith("if os.name != 'uefi':"):
    print("already wrapped")
    raise SystemExit(0)

uefi_stub = '''\
if os.name == 'uefi':
    class SSLObject:
        """UEFI: full SSLObject is omitted at import (VS2022 Shell exit after import ssl)."""
        def __init__(self, *args, **kwargs):
            raise TypeError(
                f"{self.__class__.__name__} does not have a public "
                f"constructor on UEFI."
            )

    class SSLSocket:
        """UEFI: full SSLSocket is omitted at import."""
        def __init__(self, *args, **kwargs):
            raise TypeError(
                f"{self.__class__.__name__} does not have a public "
                f"constructor on UEFI."
            )

        @classmethod
        def _create(cls, *args, **kwargs):
            raise NotImplementedError("UEFI: SSLSocket TLS support not loaded at import")

else:
'''

out = []
for i, line in enumerate(lines):
    if i == start:
        out.append(uefi_stub)
    if start <= i <= end:
        out.append("    " + line)
    else:
        out.append(line)

text = "".join(out)
text = text.replace(
    "if os.name != 'uefi':\n    # Python does not support forward declaration of types.\n    if os.name != 'uefi':\n",
    "    # Python does not support forward declaration of types.\n",
)
# Assignment belongs inside else block (indented)
text = text.replace(
    "    # Python does not support forward declaration of types.\n        SSLContext.sslsocket_class = SSLSocket\n        SSLContext.sslobject_class = SSLObject\n",
    "    # Python does not support forward declaration of types.\n    SSLContext.sslsocket_class = SSLSocket\n    SSLContext.sslobject_class = SSLObject\n",
)

p.write_text(text, encoding="utf-8")
print(f"wrapped {start + 1}-{end + 1}")
