#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/remote_files_txn_apply_dryrun_${TS}.txt"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE_FILES_SH="$ROOT_DIR/tools/remote_files.sh"

mkdir -p "$ROOT_DIR/captures"
: > "$OUT"

OPS_FILE="$(mktemp)"
REPORT_FILE="$(mktemp)"
REPORT_FILE_REPLAY="$(mktemp)"
IDEMPOTENCY_STORE="$(mktemp)"
IDEMPOTENCY_KEY="txn.dryrun.demo.1"
trap 'rm -f "$OPS_FILE" "$REPORT_FILE" "$REPORT_FILE_REPLAY" "$IDEMPOTENCY_STORE"' EXIT

cat > "$OPS_FILE" <<'EOF'
# tx apply dry-run
mkdir|/sdcard/ebins/txn_demo
upload|/sdcard/ebins/txn_demo/a.ebin
move|/sdcard/ebins/txn_demo/a.ebin|/sdcard/ebins/txn_demo/a2.ebin
delete|/sdcard/ebins/txn_demo/a2.ebin
EOF

set +e
"$REMOTE_FILES_SH" --dry-run --token dryrun-token txn-apply --ops-file "$OPS_FILE" --simulate-fail-at 3 --report-file "$REPORT_FILE" --idempotency-key "$IDEMPOTENCY_KEY" --idempotency-store "$IDEMPOTENCY_STORE" > "$OUT" 2>&1
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
grep -q "\[TXN \] summary status=rolled_back" "$OUT" || { echo "missing_txn_summary" | tee -a "$OUT"; exit 1; }

SIG="$(grep -o 'ops_signature=[0-9a-f]\{64\}' "$OUT" | head -n1 | cut -d= -f2)"
if [[ -z "$SIG" ]]; then
  echo "missing_ops_signature" | tee -a "$OUT"
  exit 1
fi

python3 - <<'PY' "$REPORT_FILE" "$SIG" >> "$OUT"
import json
import sys

report_path, sig = sys.argv[1], sys.argv[2]
with open(report_path, 'r', encoding='utf-8') as f:
    report = json.load(f)

assert report.get('schema') == 'remote_files_txn_report_v1'
assert report.get('status') == 'rolled_back'
assert report.get('ops_signature') == sig
assert report.get('steps_total') == 4
assert report.get('steps_applied') == 2
assert report.get('rollback_attempted') >= 1

print('txn_report=pass')
PY

"$REMOTE_FILES_SH" --dry-run --token dryrun-token txn-apply --ops-file "$OPS_FILE" --report-file "$REPORT_FILE_REPLAY" --idempotency-key "$IDEMPOTENCY_KEY" --idempotency-store "$IDEMPOTENCY_STORE" >> "$OUT" 2>&1

grep -q "summary status=replayed" "$OUT" || { echo "missing_replayed_summary" | tee -a "$OUT"; exit 1; }
grep -q "prior_status=rolled_back" "$OUT" || { echo "missing_replayed_prior_status" | tee -a "$OUT"; exit 1; }

python3 - <<'PY' "$REPORT_FILE_REPLAY" "$SIG" >> "$OUT"
import json
import sys

report_path, sig = sys.argv[1], sys.argv[2]
with open(report_path, 'r', encoding='utf-8') as f:
  report = json.load(f)

assert report.get('schema') == 'remote_files_txn_report_v1'
assert report.get('status') == 'replayed'
assert report.get('ops_signature') == sig

print('txn_replay_report=pass')
PY

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"
