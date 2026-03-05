#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/remote_files_txn_apply_dryrun_${TS}.txt"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE_FILES_SH="$ROOT_DIR/tools/remote_files.sh"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

OPS_FILE="$(mktemp)"
trap 'rm -f "$OPS_FILE"' EXIT

cat > "$OPS_FILE" <<'EOF'
# tx apply dry-run
mkdir|/sdcard/ebins/txn_demo
upload|/sdcard/ebins/txn_demo/a.ebin
move|/sdcard/ebins/txn_demo/a.ebin|/sdcard/ebins/txn_demo/a2.ebin
delete|/sdcard/ebins/txn_demo/a2.ebin
EOF

set +e
"$REMOTE_FILES_SH" --dry-run --token dryrun-token txn-apply --ops-file "$OPS_FILE" --simulate-fail-at 3 > "$OUT" 2>&1
RC=$?
set -e

if [[ "$RC" -eq 0 ]]; then
  echo "expected_failure=missing rc=$RC" | tee -a "$OUT"
  exit 1
fi

grep -q "\[TXN \] apply #1 mkdir" "$OUT" || { echo "missing_apply_step_1" | tee -a "$OUT"; exit 1; }
grep -q "\[TXN \] apply #2 upload" "$OUT" || { echo "missing_apply_step_2" | tee -a "$OUT"; exit 1; }
grep -q "simulated failure at step 3" "$OUT" || { echo "missing_simulated_failure" | tee -a "$OUT"; exit 1; }
grep -q "\[TXN \] rollback start" "$OUT" || { echo "missing_rollback_start" | tee -a "$OUT"; exit 1; }
grep -q "\[TXN \] rollback complete" "$OUT" || { echo "missing_rollback_complete" | tee -a "$OUT"; exit 1; }

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"
