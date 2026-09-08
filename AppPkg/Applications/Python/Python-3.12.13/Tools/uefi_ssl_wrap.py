"""One-off: wrap SSLContext subclass and SSLSocket block for UEFI in Lib/ssl.py."""
from pathlib import Path

p = Path(__file__).resolve().parents[1] / "Lib" / "ssl.py"
lines = p.read_text(encoding="utf-8").splitlines(keepends=True)


def indent_range(lines, start_1based, end_1based, insert_before):
    out = []
    for i, line in enumerate(lines):
        n = i + 1
        if n == start_1based:
            out.extend(insert_before)
        if start_1based <= n <= end_1based:
            out.append("    " + line)
        else:
            out.append(line)
    return out


lines = indent_range(
    lines,
    468,
    728,
    [
        "if os.name == 'uefi':\n",
        "    SSLContext = _SSLContext\n",
        "else:\n",
    ],
)

text = "".join(lines)
lines = text.splitlines(keepends=True)

start_i = end_i = None
for i, line in enumerate(lines):
    if line.startswith("class SSLObject:"):
        start_i = i
    if start_i is not None and line.strip() == "SSLContext.sslobject_class = SSLObject":
        end_i = i

if start_i is None or end_i is None:
    raise SystemExit("SSLObject block not found")

for j in range(start_i, end_i + 1):
    if lines[j].startswith("# Python does not support"):
        start_i = j
        break

out = []
for i, line in enumerate(lines):
    if i == start_i:
        out.append("if os.name != 'uefi':\n")
    if start_i <= i <= end_i:
        out.append("    " + line)
    else:
        out.append(line)

p.write_text("".join(out), encoding="utf-8")
print(f"patched {p}")
