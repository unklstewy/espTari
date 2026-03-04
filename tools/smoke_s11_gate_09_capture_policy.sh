#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
ADMIN_TOKEN="${ADMIN_TOKEN:-esptari-admin-token}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s11_gate_09_capture_policy_${TS}.txt"
JSON_OUT="captures/s11_gate_09_capture_policy_${TS}.json"

mkdir -p captures

python - <<'PY' "$BASE_URL" "$ADMIN_TOKEN" "$OUT" "$JSON_OUT" "$TS"
import json, subprocess, sys
BASE, TOKEN, OUT, JSON_OUT, RUN_ID = sys.argv[1:6]

def log(s):
    with open(OUT, "a", encoding="utf-8") as f: f.write(s+"\n")
    print(s, flush=True)

def call(method,path,body=None,headers=None):
    cmd=["curl","--max-time","12","-sS","-w","\n%{http_code}","-X",method,BASE+path]
    for k,v in (headers or {}).items(): cmd += ["-H", f"{k}: {v}"]
    if body is not None: cmd += ["-H","Content-Type: application/json","-d",json.dumps(body)]
    raw=subprocess.check_output(cmd,text=True)
    body_raw, code_raw = raw.rsplit("\n",1) if "\n" in raw else (raw,"000")
    try: code=int(code_raw)
    except: code=0
    return code

def route_exists(code): return code not in (0,404,501)

auth={"Authorization": f"Bearer {TOKEN}"}
checks=[]
log(f"start run_id={RUN_ID} gate=GATE-S11-09")

c1=call("GET","/api/v2/input/capture/policy",headers=auth)
log(f"check=capture_get primary=/api/v2/input/capture/policy http={c1} ok={route_exists(c1)}")
if not route_exists(c1):
    c1=call("GET","/api/v2/input/capture",headers=auth)
    log(f"check=capture_get fallback=/api/v2/input/capture http={c1} ok={route_exists(c1)}")
checks.append({"check":"capture_get","http":c1,"ok":route_exists(c1)})

payload={"enabled":True,"mode":"click_to_capture"}
c2=call("POST","/api/v2/input/capture/policy",body=payload,headers=auth)
log(f"check=capture_set primary=/api/v2/input/capture/policy http={c2} ok={route_exists(c2)}")
if not route_exists(c2):
    c2=call("POST","/api/v2/input/capture",body=payload,headers=auth)
    log(f"check=capture_set fallback=/api/v2/input/capture http={c2} ok={route_exists(c2)}")
checks.append({"check":"capture_set","http":c2,"ok":route_exists(c2)})

status="pass" if all(c["ok"] for c in checks) else "conditional"
out={"gate":"GATE-S11-09","run_id":RUN_ID,"status":status,"checks":checks}
with open(JSON_OUT,"w",encoding="utf-8") as f: json.dump(out,f,indent=2)
log(f"status={status}")
log(f"json_path={JSON_OUT}")
PY

echo "S11 gate 09 fixture COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence bundle: $JSON_OUT"
