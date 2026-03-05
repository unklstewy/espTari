#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
OUTPUT_DIR="${1:-$PROJECT_ROOT/captures}"
DB_PATH="${2:-$PROJECT_ROOT/TRACKING/tracking.db}"

TS="$(date +%Y%m%d_%H%M%S)"
OUT_JSON="$OUTPUT_DIR/ebin_216_release_packet_st520_st1040_${TS}.json"

mkdir -p "$OUTPUT_DIR"

python3 - <<'PY' "$DB_PATH" "$OUT_JSON"
import json
import sqlite3
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

db_path, out_json = sys.argv[1:3]

conn = sqlite3.connect(db_path)
conn.row_factory = sqlite3.Row

task_ids = [f"EBIN-{n}" for n in range(203, 216)]

task_rows = conn.execute(
    """
    SELECT task_id, status, objective, sprint, updated_at
    FROM tasks
    WHERE task_id IN ({})
    ORDER BY task_id
    """.format(",".join("?" for _ in task_ids)),
    task_ids,
).fetchall()

acceptance = {}
for task_id in task_ids:
    row = conn.execute(
        """
        SELECT id, task_ref, decision, notes, evidence_link, decision_date
        FROM acceptance_decisions
        WHERE task_ref = ?
        ORDER BY id DESC
        LIMIT 1
        """,
        (task_id,),
    ).fetchone()
    if row is not None:
        acceptance[task_id] = row

conn.close()

git_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
git_branch = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"], text=True).strip()

task_cards = []
for row in task_rows:
    task_id = row["task_id"]
    acc = acceptance.get(task_id)
    evidence_links = []
    if acc and acc["evidence_link"]:
        evidence_links = [item.strip() for item in acc["evidence_link"].split(",") if item.strip()]

    task_cards.append(
        {
            "task_id": task_id,
            "status": row["status"],
            "objective": row["objective"],
            "sprint": row["sprint"],
            "updated_at": row["updated_at"],
            "acceptance": {
                "decision_id": acc["id"] if acc else None,
                "decision": acc["decision"] if acc else None,
                "notes": acc["notes"] if acc else None,
                "decision_date": acc["decision_date"] if acc else None,
                "evidence_links": evidence_links,
            },
        }
    )

packet = {
    "schema": "st_release_packet_v1",
    "packet_id": f"st520_st1040_initial_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}",
    "generated_at": datetime.now(timezone.utc).isoformat(),
    "machine": "atari_st",
    "layout_version": "st520_st1040_layout_v1",
    "source_control": {
        "branch": git_branch,
        "commit": git_commit,
    },
    "bundles": [
        {
            "profile_module": "st.profile.520",
            "package_index": "/sdcard/ebins/atari_st/packages/st.profile.520/index.json",
            "package_manifest": "/sdcard/ebins/atari_st/packages/st.profile.520/manifest.json",
        },
        {
            "profile_module": "st.profile.1040",
            "package_index": "/sdcard/ebins/atari_st/packages/st.profile.1040/index.json",
            "package_manifest": "/sdcard/ebins/atari_st/packages/st.profile.1040/manifest.json",
        },
    ],
    "task_acceptance_chain": task_cards,
}

Path(out_json).write_text(json.dumps(packet, indent=2) + "\n", encoding="utf-8")
print(out_json)
PY

echo "Release packet: $OUT_JSON"