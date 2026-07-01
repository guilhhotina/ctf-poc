#!/usr/bin/env bash
set -euo pipefail

: "${XERCES_SRC:?set XERCES_SRC to the Xerces-C++ source checkout}"
: "${XERCES_BUILD:?set XERCES_BUILD to the Xerces-C++ build directory}"

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
"$CXX" "${COMMON_FLAGS[@]}" poc_xerces_rce.cpp -o poc_xerces_rce
strip -s poc_xerces_rce

printf '[+] built ./gen_uaf3\n'
printf '[+] built ./poc_xerces_rce (PIE, stripped)\n'
