import sys

NEARBY_INCLUDE = '#include "antlr4-runtime.h"'
PARSERBASE_INCLUDE = '#include "parserbase/CParserBase.h"'

with open(sys.argv[1], "r") as generated_header:
    code = generated_header.read();

lines = code.splitlines()
if PARSERBASE_INCLUDE in lines:
    exit()

for lineno, line in enumerate(lines):
    if line == NEARBY_INCLUDE:
        break
else:
    print("ERROR : Target line not found")
    exit(1)

lines.insert(lineno + 1, PARSERBASE_INCLUDE)
with open(sys.argv[1], "w") as generated_header:
    generated_header.write("\n".join(lines))
