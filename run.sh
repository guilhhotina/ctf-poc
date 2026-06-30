#!/usr/bin/env bash
set -uo pipefail

rm -f /tmp/xerces_uaf_rce_proof
./poc_xerces_rce
rc=$?
proof=$(cat /tmp/xerces_uaf_rce_proof 2>/dev/null || true)

echo "rc=$rc"
echo "proof=$proof"

if [[ "$rc" == "42" && "$proof" == "xerces-uaf-rce" ]]; then
  echo '[+] code execution proof hit'
  exit 0
fi

echo '[-] proof did not hit'
exit 1
