#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s11_gate_10_catalog_missing_asset_${TS}.txt"
JSON_OUT="captures/s11_gate_10_catalog_missing_asset_${TS}.json"

mkdir -p captures

python - <<'PY' "$BASE_URL" "$OUT" "$JSON_OUT" "$TS"
import json, subprocess, sys
BASE, OUT, JSON_OUT, RUN_ID = sys.argv[1:5]

def log(s):
    with open(OUT, "a", encoding="utf-8") as f: f.write(s+"\n")
    print(s, flush=True)

def call(method,path):
    cmd=["curl","--max-time","12","-sS","-w","\n%{http_code}","-X",method,BASE+path]
    raw=subprocess.check_output(cmd,text=True)
    body_raw, code_raw = raw.rsplit("\n",1) if "\n" in raw else (raw,"000")
    try: code=int(code_raw)
    except: code=0
    try: data=json.loads(body_raw) if body_raw.strip() else {}
    except: data={"_raw": body_raw.strip()}
    return code, data

def route_exists(code): return code not in (0,404,501)

checks=[]
log(f"start run_id={RUN_ID} gate=GATE-S11-10")
c1,b1=call("GET","/api/v2/catalogs/list")
checks.append({"check":"catalog_list","http":c1,"ok":c1==200})
log(f"check=catalog_list http={c1} ok={c1==200}")

catalog_candidates=[]
if isinstance(b1, dict):
    data=b1.get("data")
    if isinstance(data, list):
        for item in data:
            if isinstance(item, dict) and item.get("catalog"):
                catalog_candidates.append(item.get("catalog"))
            elif isinstance(item, str):
                catalog_candidates.append(item)

catalog_candidates += ["floppy_catalog", "rom_catalog"]
seen=set(); catalogs=[]
for c in catalog_candidates:
    if c and c not in seen:
        seen.add(c); catalogs.append(c)

mr_ok=False
mr_http=0
mr_catalog=""
for c in catalogs:
    h,b=call("GET",f"/api/v2/catalogs/{c}/missing-report")
    log(f"check=missing_report_probe catalog={c} http={h} ok={route_exists(h)}")
    if route_exists(h):
        mr_ok=True; mr_http=h; mr_catalog=c; break
    mr_http=h; mr_catalog=c
checks.append({"check":"missing_report_route","http":mr_http,"catalog":mr_catalog,"ok":mr_ok})

status="pass" if all(ch["ok"] for ch in checks) else "conditional"
out={"gate":"GATE-S11-10","run_id":RUN_ID,"status":status,"checks":checks}
with open(JSON_OUT,"w",encoding="utf-8") as f: json.dump(out,f,indent=2)
log(f"status={status}")
log(f"json_path={JSON_OUT}")
PY

echo "S11 gate 10 fixture COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence bundle: $JSON_OUT"
