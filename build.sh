#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
usage:
  XERCES_SRC=/path/to/xerces-c \
  XERCES_BUILD=/path/to/xerces-build \
  ./build.sh

expected files:
  $XERCES_SRC/src/xercesc/parsers/SAXParser.hpp
  $XERCES_BUILD/src/libxerces-c-4.0.so
  $XERCES_BUILD/src/xercesc/util/Xerces_autoconf_config.hpp
USAGE
}

: "${XERCES_SRC:?set XERCES_SRC to the Xerces-C++ source checkout}"
: "${XERCES_BUILD:?set XERCES_BUILD to the Xerces-C++ build directory}"

if [[ ! -f "$XERCES_SRC/src/xercesc/parsers/SAXParser.hpp" ]]; then
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

"$CXX" -std=c++17 -O2 \
  -I"$XERCES_SRC/src" \
  -I"$XERCES_BUILD/src" \
  poc_xerces_rce.cpp \
  "$XERCES_BUILD/src/libxerces-c-4.0.so" \
  -Wl,-rpath,"$XERCES_BUILD/src" \
  -o poc_xerces_rce

printf '[+] built ./poc_xerces_rce\n'
ldd ./poc_xerces_rce | grep xerces || true
