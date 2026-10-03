# Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com
"""Check real generated-object references against the ELF bridge exports."""
from pathlib import Path
import re
import subprocess
import sys


def symbols(*arguments):
    text = subprocess.check_output(["nm", *map(str, arguments)], text=True)
    return set(re.findall(r"\b((?:HTN|Htn)\w+)(?:@\S+)?$", text, re.MULTILINE))


objects = list(Path(sys.argv[1]).rglob("*.o"))
if not objects:
    sys.exit("No domain objects to inspect")
required = set().union(*(symbols("-u", path) for path in objects))
numeric = set().union(*(symbols("-u", path) for path in objects if "coverage.generated" in path.name))
if not {"HTNAtom_SetInt", "HTNAtom_SetFloat"} <= numeric:
    sys.exit("Numeric regression fixture lost its bridge references")
exports = symbols("-D", "--defined-only", sys.argv[2])
if not required or required - exports:
    sys.exit(f"Missing bridge exports: {sorted(required - exports)}")
print(f"PASS: {len(required)} external HTN symbols available in ELF bridge")
