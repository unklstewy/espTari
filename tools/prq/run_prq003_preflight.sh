#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="$ROOT_DIR/captures"
mkdir -p "$OUT_DIR"
OUT_FILE="${OUT_FILE:-$OUT_DIR/prq003_preflight_$(date +%Y%m%d_%H%M%S).txt}"

PASS=0
FAIL=0

check_file() {
  local label="$1"
  local path="$2"
  if [[ -f "$path" ]]; then
    echo "PASS file $label: $path" >> "$OUT_FILE"
    PASS=$((PASS + 1))
  else
    echo "FAIL file $label: $path" >> "$OUT_FILE"
    FAIL=$((FAIL + 1))
  fi
}

check_cmd() {
  local label="$1"
  local cmd="$2"
  if command -v "$cmd" >/dev/null 2>&1; then
    echo "PASS cmd $label: $(command -v "$cmd")" >> "$OUT_FILE"
    PASS=$((PASS + 1))
  else
    echo "FAIL cmd $label: $cmd" >> "$OUT_FILE"
    FAIL=$((FAIL + 1))
  fi
}

{
  echo "root=$ROOT_DIR"
  echo "started_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "git_commit=$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)"
  echo "git_branch=$(git -C "$ROOT_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
} > "$OUT_FILE"

check_cmd "python3" "python3"
check_cmd "cmake" "cmake"
check_cmd "ninja" "ninja"

check_file "sdkconfig" "$ROOT_DIR/sdkconfig"
check_file "partitions_csv" "$ROOT_DIR/partitions.csv"
check_file "build_ninja" "$ROOT_DIR/build/build.ninja"
check_file "firmware_bin" "$ROOT_DIR/build/espTari.bin"
check_file "firmware_elf" "$ROOT_DIR/build/espTari.elf"
check_file "partition_table_bin" "$ROOT_DIR/build/partition_table/partition-table.bin"

{
  echo "pass_count=$PASS"
  echo "fail_count=$FAIL"
  echo "completed_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >> "$OUT_FILE"

if [[ "$FAIL" -ne 0 ]]; then
  echo "PRQ-003 preflight completed with failures. See: $OUT_FILE" >&2
  exit 1
fi

echo "PRQ-003 preflight passed. Evidence: $OUT_FILE"
