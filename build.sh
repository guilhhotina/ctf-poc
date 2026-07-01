#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
usage:
  XERCES_SRC=/path/to/xerces-c-stock-clean \
  XERCES_BUILD=/path/to/xerces-c-stock-build \
  ./build.sh
USAGE
}

: "${XERCES_SRC:?set XERCES_SRC to the Xerces-C++ source checkout}"
: "${XERCES_BUILD:?set XERCES_BUILD to the Xerces-C++ build directory}"

if [[ ! -f "$XERCES_SRC/src/xercesc/parsers/XercesDOMParser.hpp" ]]; then
  echo "[-] invalid XERCES_SRC: $XERCES_SRC" >&2
  usage >&2
  exit 1
fi

if [[ ! -f "$XERCES_BUILD/src/libxerces-c-4.0.so" ]]; then
  echo "[-] invalid XERCES_BUILD: $XERCES_BUILD" >&2
  usage >&2
  exit 1
fi

CXX=${CXX:-g++}
COMMON_FLAGS=(
  -std=c++17
  -O2
  -I"$XERCES_SRC/src"
  -I"$XERCES_BUILD/src"
  "$XERCES_BUILD/src/libxerces-c-4.0.so"
  -Wl,-rpath,"$XERCES_BUILD/src"
)

"$CXX" "${COMMON_FLAGS[@]}" gen_uaf3.cpp -o gen_uaf3
"$CXX" -fno-pie -no-pie "${COMMON_FLAGS[@]}" poc_xerces_rce.cpp -o poc_xerces_rce

printf '[+] built ./gen_uaf3\n'
printf '[+] built ./poc_xerces_rce\n'
