# Xerces-C++ serialized grammar reload RCE PoC

tiny proof package for Apache Xerces-C++ at commit
`53c0401812bfe5523594c1180f5ac7c758a2eaf7`.

this is the final stock-only PoC for the ctf challenge. it does **not** rely on a custom reclaimed object or an instrumented Xerces build.

the exploit starts from a **valid serialized schema grammar**, mutates the object graph so an adopted container frees a `SchemaAttDef` while its object id stays reachable in the deserialize load pool, then reclaims that freed chunk with a later `QName.local` string allocation. a mutated `SchemaElementDecl::fAttWildCard` points at the dangling id, and stock Xerces eventually performs a virtual `delete` through the reclaimed bytes.

the proof writes `/tmp/xerces_serialized_rce_proof` and exits with `42`.

## files

- `poc_xerces_rce.cpp` - final stock-api trigger program
- `gen_uaf3.cpp` - generates the valid serialized grammar base
- `build_final_input.py` - mutates the valid grammar into the final proof input
- `trigger.xml` - instance parsed after grammar reload
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
deserialized
parsed
[+] xerces rce
rc=42
proof=xerces-serialized-rce
[+] code execution proof hit
```

## required parser setup

the proof uses public Xerces APIs only:

```cpp
parser.setValidationScheme(XercesDOMParser::Val_Always);
parser.setDoNamespaces(true);
parser.setDoSchema(true);
parser.useCachedGrammarInParse(true);
parser.setValidationSchemaFullChecking(true);
```

`cacheGrammarFromParse(true)` is used while generating the valid serialized grammar.

## root cause summary

1. start from a valid serialized schema grammar
2. mutate two `SchemaAttDef` names inside the same adopted `RefHash2KeysTableOf<SchemaAttDef>` so they collide
3. during `RefHash2KeysTableOf::put()`, the second insert frees the first `SchemaAttDef` (`id=15`)
4. the freed object id still remains reachable through the deserialize load pool
5. a later `QName.local` allocation with `bufferLen=56` reuses the exact freed `SchemaAttDef` chunk
6. mutate `SchemaElementDecl::fAttWildCard` to reference object id `15`
7. stock Xerces later executes `delete fAttWildCard`, which dispatches through the reclaimed bytes as a fake vtable
8. control flow reaches the proof function in the non-PIE trigger binary

## why this follows the ctf rules

- stock upstream Xerces is unmodified in the final proof
- the input starts from a valid serialized grammar
- the trigger uses grammar caching / serialized grammar reload, not only plain XML parsing
- the effect is real code execution, not just a crash or parser exception
- the final proof does not depend on a custom reclaimed C++ object inside the exploit path

## note on the input file

`final_input.bin` is generated at run time because the fake vtable pointer must match the fixed address of `fake_vtable` in the non-PIE proof binary.
