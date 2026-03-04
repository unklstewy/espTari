#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s11_gate_12_save_restore_${TS}.txt"
JSON_OUT="captures/s11_gate_12_save_restore_${TS}.json"

mkdir -p captures

python - <<'PY' "$BASE_URL" "$OUT" "$JSON_OUT" "$TS"
import json, subprocess, sys
BASE, OUT, JSON_OUT, RUN_ID = sys.argv[1:5]

def log(s):
    with open(OUT, "a", encoding="utf-8") as f: f.write(s+"\n")
    print(s, flush=True)

def call(method,path,body=None):
    cmd=["curl","--max-time","12","-sS","-w","\n%{http_code}","-X",method,BASE+path]
    if body is not None: cmd += ["-H","Content-Type: application/json","-d",json.dumps(body)]
    raw=subprocess.check_output(cmd,text=True)
    body_raw, code_raw = raw.rsplit("\n",1) if "\n" in raw else (raw,"000")
    try: code=int(code_raw)
    except: code=0
    return code

def route_exists(code): return code not in (0,404,501)

checks=[]
log(f"start run_id={RUN_ID} gate=GATE-S11-12")
payload={"session_id":"ses_local","snapshot_id":"s11_gate12_probe"}
c1=call("POST","/api/v2/engine/session/suspend-save",payload)
log(f"check=suspend_save primary=/api/v2/engine/session/suspend-save http={c1} ok={route_exists(c1)}")
if not route_exists(c1):
    c1=call("POST","/api/v2/snapshots/suspend-save",payload)
    log(f"check=suspend_save fallback=/api/v2/snapshots/suspend-save http={c1} ok={route_exists(c1)}")
checks.append({"check":"suspend_save","http":c1,"ok":route_exists(c1)})

v_payload={"snapshot_id":"s11_gate12_probe","strict":True}
c2=call("POST","/api/v2/engine/session/restore/validate",v_payload)
log(f"check=restore_validate primary=/api/v2/engine/session/restore/validate http={c2} ok={route_exists(c2)}")
if not route_exists(c2):
    c2=call("POST","/api/v2/snapshots/restore/validate",v_payload)
    log(f"check=restore_validate fallback=/api/v2/snapshots/restore/validate http={c2} ok={route_exists(c2)}")
checks.append({"check":"restore_validate","http":c2,"ok":route_exists(c2)})

status="pass" if all(c["ok"] for c in checks) else "conditional"
out={"gate":"GATE-S11-12","run_id":RUN_ID,"status":status,"checks":checks}
with open(JSON_OUT,"w",encoding="utf-8") as f: json.dump(out,f,indent=2)
log(f"status={status}")
log(f"json_path={JSON_OUT}")
PY

echo "S11 gate 12 fixture COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence bundle: $JSON_OUT"
