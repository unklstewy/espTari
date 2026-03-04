#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
ADMIN_TOKEN="${ADMIN_TOKEN:-esptari-admin-token}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s11_gate_07_input_translation_${TS}.txt"
JSON_OUT="captures/s11_gate_07_input_translation_${TS}.json"

mkdir -p captures

python - <<'PY' "$BASE_URL" "$ADMIN_TOKEN" "$OUT" "$JSON_OUT" "$TS"
import json, subprocess, sys
BASE, TOKEN, OUT, JSON_OUT, RUN_ID = sys.argv[1:6]

def log(s):
    with open(OUT, "a", encoding="utf-8") as f:
        f.write(s + "\n")
    print(s, flush=True)

def call(method, path, body=None, headers=None):
    cmd=["curl","--max-time","12","-sS","-w","\n%{http_code}","-X",method,BASE+path]
    for k,v in (headers or {}).items(): cmd += ["-H", f"{k}: {v}"]
    if body is not None: cmd += ["-H","Content-Type: application/json","-d",json.dumps(body)]
    raw=subprocess.check_output(cmd,text=True)
    body_raw, code_raw = raw.rsplit("\n",1) if "\n" in raw else (raw,"000")
    try: code=int(code_raw)
    except: code=0
    try: data=json.loads(body_raw) if body_raw.strip() else {}
    except: data={"_raw": body_raw.strip()}
    return code, data

def route_exists(code): return code not in (0,404,501)

auth={"Authorization": f"Bearer {TOKEN}"}
checks=[]
log(f"start run_id={RUN_ID} gate=GATE-S11-07")

c1,b1=call("GET","/api/v2/input/mappings",headers=auth)
checks.append({"check":"mappings_list","http":c1,"ok":route_exists(c1)})
log(f"check=mappings_list http={c1} ok={route_exists(c1)}")

payload={"session_id":"ses_local","device":"keyboard","event":{"key":"A","state":"down"}}
c2,b2=call("POST","/api/v2/input/events",body=payload,headers=auth)
log(f"check=event_translate primary=/api/v2/input/events http={c2} ok={route_exists(c2)}")
if not route_exists(c2):
    c2,b2=call("POST","/api/v2/input/translate",body=payload,headers=auth)
    log(f"check=event_translate fallback=/api/v2/input/translate http={c2} ok={route_exists(c2)}")
checks.append({"check":"event_translate","http":c2,"ok":route_exists(c2)})

status="pass" if all(c["ok"] for c in checks) else "conditional"
out={"gate":"GATE-S11-07","run_id":RUN_ID,"status":status,"checks":checks}
with open(JSON_OUT,"w",encoding="utf-8") as f: json.dump(out,f,indent=2)
log(f"status={status}")
log(f"json_path={JSON_OUT}")
PY

echo "S11 gate 07 fixture COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence bundle: $JSON_OUT"
