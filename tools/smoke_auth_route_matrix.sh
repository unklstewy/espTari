#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://192.168.1.196}"
AUTH_CLIENT_ID="${AUTH_CLIENT_ID:-esptari-smoke}"
AUTH_CLIENT_SECRET="${AUTH_CLIENT_SECRET:-esptari-smoke-secret}"
WRONG_SCOPE="${WRONG_SCOPE:-scope:wrong}"

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/auth_route_matrix_${TS}.txt"
mkdir -p captures

BASE_URL="$BASE_URL" AUTH_CLIENT_ID="$AUTH_CLIENT_ID" AUTH_CLIENT_SECRET="$AUTH_CLIENT_SECRET" WRONG_SCOPE="$WRONG_SCOPE" OUT="$OUT" \
/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python - <<'PY'
import json
import os
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

base = os.environ.get("BASE_URL", "http://192.168.1.196")
client_id = os.environ.get("AUTH_CLIENT_ID", "esptari-smoke")
client_secret = os.environ.get("AUTH_CLIENT_SECRET", "esptari-smoke-secret")
wrong_scope = os.environ.get("WRONG_SCOPE", "scope:wrong")
out = Path(os.environ.get("OUT", f"captures/auth_route_matrix_{time.strftime('%Y%m%d_%H%M%S')}.txt"))
out.parent.mkdir(parents=True, exist_ok=True)

rg_cmd = r'''rg -n "esptari_web_auth_register_protected_route\(" components/esptari_web | sed -En 's/.*register_protected_route\(server_handle, "([^"]+)", (HTTP_[A-Z]+), .*"([^"]+)"\).*/\1|\2|\3/p' | sort -u'''
res = subprocess.run(rg_cmd, shell=True, text=True, capture_output=True)
routes = [line.strip().split('|') for line in res.stdout.splitlines() if '|HTTP_' in line]


def post_token(scope: str) -> str:
    payload = json.dumps(
        {
            "grant_type": "client_credentials",
            "client_id": client_id,
            "client_secret": client_secret,
            "scope": scope,
        }
    ).encode()
    req = urllib.request.Request(
        base + "/api/v2/auth/token",
        data=payload,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        data = json.loads(resp.read().decode())
    return data.get("data", {}).get("access_token", "")


def call(uri: str, method: str, token=None) -> int:
    call_uri = uri.replace('*', 'test-id')
    url = base + call_uri
    data = None
    headers = {}
    if method != "GET":
        data = b"{}"
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            return resp.getcode()
    except urllib.error.HTTPError as e:
        return e.code
    except Exception:
        return 0


wrong_token = post_token(wrong_scope)
if not wrong_token:
    raise SystemExit("failed_to_mint_wrong_scope_token")

scope_tokens = {}
pass_n = 0
fail_n = 0
lines = []
lines.append(f"base_url={base}")
lines.append(f"total_routes={len(routes)}")

for uri, http_method, scope in routes:
    method = http_method.replace("HTTP_", "")
    code_noauth = call(uri, method, None)
    code_wrong = call(uri, method, wrong_token)

    token = scope_tokens.get(scope)
    if not token:
        token = post_token(scope)
        scope_tokens[scope] = token

    code_right = call(uri, method, token)

    ok = code_noauth == 401 and code_wrong == 403 and code_right not in (0, 401, 403)
    if ok:
        pass_n += 1
        lines.append(f"PASS|{uri}|{http_method}|{scope}|{code_noauth}|{code_wrong}|{code_right}")
    else:
        fail_n += 1
        lines.append(f"FAIL|{uri}|{http_method}|{scope}|{code_noauth}|{code_wrong}|{code_right}")

lines.append(f"summary_pass={pass_n}")
lines.append(f"summary_fail={fail_n}")
out.write_text("\n".join(lines) + "\n")

print(f"evidence={out}")
print(f"summary_pass={pass_n}")
print(f"summary_fail={fail_n}")
PY
