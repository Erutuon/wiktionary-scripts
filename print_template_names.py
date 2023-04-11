#! /usr/bin/env python3

# Prints template names from JSONL with schema of { "title": page_title, "templates": [ template_1, template_2, ... ]}

import regex as re
import orjson as json
from sys import argv

if len(argv) < 2:
    raise TypeError("Supply filepath in parameter 1")
filepath = argv[1]

with open(filepath) as f:
    data = dict()
    for line in f:
        try:
            d = json.loads(line)
        except Exception as e:
            print(f"Could not load line {line}: {e}")
        data[d["title"]] = d["templates"]

s = set()
for k in data:
    for template in data[k]:
        s.add(re.match(r"\{\{(.+?)\|", template).group(1))

format = argv[2] if len(argv) >= 3 else "{{{{temp|{}}}}}"
l = [format.format(t) for t in s]
l.sort()
print(", ".join(l))
