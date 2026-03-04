#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SAMPLES="${SAMPLES:-20}"
INTERVAL_SEC="${INTERVAL_SEC:-0.5}"
PROGRESS_EVERY="${PROGRESS_EVERY:-1}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s11_gate_13_sustained_slo_${TS}.txt"
JSON_OUT="captures/s11_gate_13_sustained_slo_${TS}.json"

mkdir -p captures

python - <<'PY' "$BASE_URL" "$SAMPLES" "$INTERVAL_SEC" "$PROGRESS_EVERY" "$OUT" "$JSON_OUT" "$TS"
import json, subprocess, sys, time
BASE=sys.argv[1]
SAMPLES=int(sys.argv[2])
INTERVAL=float(sys.argv[3])
PROGRESS_EVERY=max(1,int(sys.argv[4]))
OUT=sys.argv[5]
JSON_OUT=sys.argv[6]
RUN_ID=sys.argv[7]

THRESHOLDS={"input_latency_ms":50.0,"jitter_ms":30.0,"dropped_frame_rate_pct":1.0}

def log(s):
    with open(OUT,"a",encoding="utf-8") as f: f.write(s+"\n")
    print(s, flush=True)

def call(path):
    cmd=["curl","--max-time","12","-sS","-w","\n%{http_code}",BASE+path]
    raw=subprocess.check_output(cmd,text=True)
    body_raw, code_raw = raw.rsplit("\n",1) if "\n" in raw else (raw,"000")
    try: code=int(code_raw)
    except: code=0
    try: data=json.loads(body_raw) if body_raw.strip() else {}
    except: data={"_raw": body_raw.strip()}
    return code,data

def route_exists(code): return code not in (0,404,501)

def extract_metrics(payload):
    if not isinstance(payload,dict): return {}
    d=payload.get("data")
    if isinstance(d,dict):
        return d
    return payload

route_checks=[]
log(f"start run_id={RUN_ID} samples={SAMPLES} interval_sec={INTERVAL} progress_every={PROGRESS_EVERY}")
for path in ["/api/v2/metrics/performance","/api/v2/metrics/performance/samples?limit=5","/api/v2/metrics/performance/thresholds"]:
    c,_=call(path)
    route_checks.append({"path":path,"http":c,"ok":route_exists(c)})
    log(f"route_check path={path} http={c} ok={route_exists(c)}")

series=[]
started=time.time()
for i in range(SAMPLES):
    c,p=call("/api/v2/metrics/performance")
    m=extract_metrics(p)
    sample={"idx":i,"http":c,"metrics":{}}
    for k in ["input_latency_ms","jitter_ms","dropped_frame_rate_pct"]:
        v=m.get(k)
        if isinstance(v,(int,float)):
            sample["metrics"][k]=float(v)
    series.append(sample)
    if (i + 1) % PROGRESS_EVERY == 0 or (i + 1) == SAMPLES:
        elapsed=time.time()-started
        done=i+1
        eta=((elapsed/done)*(SAMPLES-done)) if done > 0 else 0.0
        il=sample["metrics"].get("input_latency_ms")
        jt=sample["metrics"].get("jitter_ms")
        df=sample["metrics"].get("dropped_frame_rate_pct")
        log(
            "progress "
            f"sample={done}/{SAMPLES} "
            f"http={c} "
            f"input_latency_ms={il if il is not None else 'na'} "
            f"jitter_ms={jt if jt is not None else 'na'} "
            f"dropped_frame_rate_pct={df if df is not None else 'na'} "
            f"elapsed_sec={elapsed:.1f} eta_sec={eta:.1f}"
        )
    time.sleep(INTERVAL)

valid=[s for s in series if s["metrics"]]
agg={}
for k in ["input_latency_ms","jitter_ms","dropped_frame_rate_pct"]:
    vals=[s["metrics"][k] for s in valid if k in s["metrics"]]
    if vals:
        agg[k]={"min":min(vals),"max":max(vals),"avg":sum(vals)/len(vals),"count":len(vals),"threshold":THRESHOLDS[k]}

routes_ok=all(r["ok"] for r in route_checks)
hard_eval_possible=all(k in agg for k in THRESHOLDS.keys()) and all(agg[k]["count"]>=max(5,SAMPLES//2) for k in THRESHOLDS.keys())
if hard_eval_possible:
    hard_pass=(agg["input_latency_ms"]["max"]<=THRESHOLDS["input_latency_ms"] and agg["jitter_ms"]["max"]<THRESHOLDS["jitter_ms"] and agg["dropped_frame_rate_pct"]["max"]<THRESHOLDS["dropped_frame_rate_pct"])
    status="pass" if hard_pass else "fail"
else:
    status="conditional" if routes_ok else "fail"

out={
    "gate":"GATE-S11-13",
    "run_id":RUN_ID,
    "samples_requested":SAMPLES,
    "interval_sec":INTERVAL,
    "status":status,
    "route_checks":route_checks,
    "metrics_aggregate":agg,
    "hard_eval_possible":hard_eval_possible
}
with open(JSON_OUT,"w",encoding="utf-8") as f: json.dump(out,f,indent=2)
log("aggregate=" + json.dumps(agg, sort_keys=True))
log(f"status={status}")
log(f"hard_eval_possible={hard_eval_possible}")
log(f"json_path={JSON_OUT}")
PY

echo "S11 gate 13 sustained SLO fixture COMPLETE"
echo "Evidence log: $OUT"
echo "Evidence bundle: $JSON_OUT"
