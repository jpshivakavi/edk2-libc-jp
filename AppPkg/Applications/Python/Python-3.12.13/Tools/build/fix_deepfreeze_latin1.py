import re
from pathlib import Path

p = Path(__file__).resolve().parents[2] / "Python" / "deepfreeze" / "deepfreeze.c"
text = p.read_text(encoding="utf-8")
text = re.sub(r"&_Py_LATIN1_CHR\((\'.\')\)", r"_Py_LATIN1_CHR(\1)", text)
text = re.sub(r"&_Py_ID\((.)\)", r"_Py_LATIN1_CHR('\1')", text)
p.write_text(text, encoding="utf-8")
left = re.findall(r"&_Py_ID\(.\)", text)
print("done, remaining single-char &_Py_ID:", len(left))
