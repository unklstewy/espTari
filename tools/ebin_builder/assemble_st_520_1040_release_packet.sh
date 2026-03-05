#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
OUTPUT_DIR="${1:-$PROJECT_ROOT/captures}"
DB_PATH="${2:-$PROJECT_ROOT/TRACKING/tracking.db}"

TS="$(date +%Y%m%d_%H%M%S)"
OUT_JSON="$OUTPUT_DIR/ebin_216_release_packet_st520_st1040_${TS}.json"
OUT_PROVENANCE_JSON="$OUTPUT_DIR/ebin_220_release_provenance_st520_st1040_${TS}.json"
OUT_PROVENANCE_SIG="$OUT_PROVENANCE_JSON.sig"

mkdir -p "$OUTPUT_DIR"

python3 - <<'PY' "$DB_PATH" "$OUT_JSON" "$OUT_PROVENANCE_JSON" "$OUT_PROVENANCE_SIG"
import json
import sqlite3
import subprocess
import sys
import hashlib
from datetime import datetime, timezone
from pathlib import Path

db_path, out_json, out_provenance_json, out_provenance_sig = sys.argv[1:5]

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

provenance = {
    "schema": "st_release_provenance_manifest_v1",
    "manifest_version": 1,
    "generated_at": datetime.now(timezone.utc).isoformat(),
    "machine": "atari_st",
    "bundle_set": "st520_st1040",
    "source_control": {
        "branch": git_branch,
        "commit": git_commit,
    },
    "bundles": [
        {
            "profile_module": "st.profile.520",
            "package_index": "/sdcard/ebins/atari_st/packages/st.profile.520/index.json",
            "package_manifest": "/sdcard/ebins/atari_st/packages/st.profile.520/manifest.json",
            "machine_profile_ebin": "/sdcard/ebins/atari_st/machine_profile/st.profile.520-1.0.0.ebin",
            "machine_profile_signature": "/sdcard/ebins/atari_st/machine_profile/st.profile.520-1.0.0.ebin.sig",
        },
        {
            "profile_module": "st.profile.1040",
            "package_index": "/sdcard/ebins/atari_st/packages/st.profile.1040/index.json",
            "package_manifest": "/sdcard/ebins/atari_st/packages/st.profile.1040/manifest.json",
            "machine_profile_ebin": "/sdcard/ebins/atari_st/machine_profile/st.profile.1040-1.0.0.ebin",
            "machine_profile_signature": "/sdcard/ebins/atari_st/machine_profile/st.profile.1040-1.0.0.ebin.sig",
        },
    ],
    "release_packet": {
        "path": out_json,
        "sha256": hashlib.sha256(Path(out_json).read_bytes()).hexdigest(),
    },
    "task_acceptance_refs": task_ids,
}

provenance_text = json.dumps(provenance, indent=2) + "\n"
Path(out_provenance_json).write_text(provenance_text, encoding="utf-8")
provenance_digest = hashlib.sha256(provenance_text.encode("utf-8")).hexdigest()
Path(out_provenance_sig).write_text(
    f"ESPTARI-DEV-SIG:release_provenance@1.0.0 sha256={provenance_digest}\n",
    encoding="utf-8",
)

print(out_json)
print(out_provenance_json)
print(out_provenance_sig)
PY

echo "Release packet: $OUT_JSON"
echo "Release provenance: $OUT_PROVENANCE_JSON"
echo "Release provenance signature: $OUT_PROVENANCE_SIG"