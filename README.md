# Xerces-C++ serialized grammar reload RCE PoC

tiny proof package for Apache Xerces-C++ at commit
`53c0401812bfe5523594c1180f5ac7c758a2eaf7`.

this is the final stock-only PoC for the ctf challenge. it does not rely on a custom reclaimed object, an instrumented Xerces build, or exploit symbols inside the trigger binary.

the exploit starts from a valid serialized schema grammar and then mutates the object graph so an adopted container frees a `SchemaAttDef` while its object id stays reachable in the deserialize load pool, then reclaims that freed chunk with a later `QName.local` string allocation. a mutated `SchemaElementDecl::fAttWildCard` points at the dangling id, and stock Xerces eventually performs a virtual `delete` through the reclaimed bytes.

`build_final_input.py` derives the deterministic heap/libc addresses offline under `setarch -R`, writes the fake vtable and `system()` pointer directly into the serialized grammar, and creates the matching `PATH` command whose name is derived from the fake vtable pointer. the final run has no debugger, no signal handler, and no in-process helper; it is just the plain loader plus the generated serialized grammar.

the proof writes `/tmp/xerces_serialized_rce_proof` via a matching command in `PATH`.

## files

- `poc_xerces_rce.cpp` - grammar loader (PIE, stripped, no exploit symbols)
- `gen_uaf3.cpp` - generates the valid serialized grammar base
- `build_final_input.py` - mutates the valid grammar into the final proof input
- `build.sh` - builds the helper binaries against a local stock Xerces build
- `run.sh` - generates the proof input, runs it, and checks the marker file

## build

point the script at the pinned Xerces checkout and build dir:

```bash
XERCES_SRC=/path/to/xerces-c-stock-clean \
XERCES_BUILD=/path/to/xerces-c-stock-build \
./build.sh
```

expected files:

- `$XERCES_SRC/src/xercesc/parsers/XercesDOMParser.hpp`
- `$XERCES_BUILD/src/libxerces-c-4.0.so`

## run

```bash
XERCES_BUILD=/path/to/xerces-c-stock-build ./run.sh
```

expected output shape:

```text
probe this=0x...
fake_vtable=0x...
deserialized
rc=0
proof=xerces-stock-system-rce
[+] code execution proof hit through system()
```

## required parser setup

the proof uses public Xerces APIs only:

```cpp
pool.deserializeGrammars(&in);
```

`cacheGrammarFromParse(true)` is used while generating the valid serialized grammar.

## root cause summary

1. start from a valid serialized schema grammar
2. mutate two `SchemaAttDef` names inside the same adopted `RefHash2KeysTableOf<SchemaAttDef>` so they collide
3. during `RefHash2KeysTableOf::put()`, the second insert frees the first `SchemaAttDef` (`id=15`)
4. the freed object id still remains reachable through the deserialize load pool
5. a later `QName.local` allocation reuses the exact freed `SchemaAttDef` chunk with attacker-controlled content
6. mutate `SchemaElementDecl::fAttWildCard` to reference object id `15`
7. stock Xerces later executes `delete fAttWildCard`, which dispatches through the reclaimed bytes
8. the reclaimed bytes contain a fake vtable that dispatches into libc `system()` during stock cleanup

## note on reproduction

`run.sh` uses `setarch -R` for deterministic ASLR-off reproduction and a fixed grammar path (`/tmp/xerces_final_input.bin`) so the probed layout matches the final run. gdb is used only by `build_final_input.py` while generating the input; the final execution run is not under gdb and has no signal handler or in-process patching.
