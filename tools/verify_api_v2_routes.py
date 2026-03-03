#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB_DIR = ROOT / "components" / "esptari_web"

EXPECTED_ROUTES = {
    ("GET", "/api/v2/engine/health"),
    ("GET", "/api/v2/engine/status"),
    ("GET", "/api/v2/engine/session"),
    ("POST", "/api/v2/engine/session"),
    ("POST", "/api/v2/engine/session/start"),
    ("POST", "/api/v2/engine/session/pause"),
    ("POST", "/api/v2/engine/session/resume"),
    ("POST", "/api/v2/engine/session/stop"),
    ("POST", "/api/v2/engine/session/reset"),
    ("POST", "/api/v2/engine/session/suspend-save"),
    ("POST", "/api/v2/engine/session/restore-resume"),
    ("POST", "/api/v2/engine/state/restore/validate"),
    ("POST", "/api/v2/engine/state/save"),
    ("POST", "/api/v2/engine/state/restore"),
    ("GET", "/api/v2/engine/state/list"),
    ("GET", "/api/v2/engine/stream"),
    ("POST", "/api/v2/media/rom/attach"),
    ("POST", "/api/v2/media/disk/attach"),
    ("POST", "/api/v2/media/disk/eject"),
    ("POST", "/api/v2/media/cartridge/attach"),
    ("POST", "/api/v2/media/cartridge/eject"),
    ("POST", "/api/v2/debug/clock/mode"),
    ("POST", "/api/v2/debug/clock/step"),
    ("GET", "/api/v2/debug/clock/state"),
    ("GET", "/api/v2/input/mappings"),
    ("GET", "/api/v2/input/devices"),
    ("POST", "/api/v2/input/events/inject"),
    ("GET", "/api/v2/input/capture/state"),
    ("POST", "/api/v2/input/policy/enabled"),
    ("POST", "/api/v2/input/capture/config"),
    ("POST", "/api/v2/input/capture/release"),
    ("POST", "/api/v2/input/mappings/load"),
    ("POST", "/api/v2/input/mappings/update"),
    ("GET", "/api/v2/input/stream"),
    ("POST", "/api/v2/input/mappings"),
    ("GET", "/api/v2/input/mappings/active"),
    ("POST", "/api/v2/input/mappings/apply"),
    ("GET", "/api/v2/input/mappings/*"),
    ("PATCH", "/api/v2/input/mappings/*"),
    ("DELETE", "/api/v2/input/mappings/*"),
    ("GET", "/api/v2/stream/video"),
    ("GET", "/api/v2/stream/audio"),
    ("GET", "/api/v2/stream/telemetry/backpressure"),
    ("GET", "/api/v2/inspect/registers/stream"),
    ("GET", "/api/v2/inspect/bus/stream"),
    ("GET", "/api/v2/inspect/memory/stream"),
    ("GET", "/api/v2/inspect/registers/snapshot"),
    ("GET", "/api/v2/inspect/bus/snapshot"),
    ("GET", "/api/v2/inspect/memory/snapshot"),
    ("POST", "/api/v2/engine/checkpoint/create"),
    ("POST", "/api/v2/engine/checkpoint/load"),
    ("GET", "/api/v2/metrics/performance"),
    ("GET", "/api/v2/metrics/performance/history"),
    ("POST", "/api/v2/metrics/performance/collectors/config"),
    ("GET", "/api/v2/metrics/performance/samples"),
    ("GET", "/api/v2/metrics/performance/thresholds"),
    ("GET", "/api/v2/metrics/performance/alarms"),
    ("GET", "/api/v2/catalogs/list"),
    ("GET", "/api/v2/catalogs/*"),
    ("POST", "/api/v2/catalogs/*"),
    ("GET", "/api/v2/files/list"),
    ("POST", "/api/v2/files/upload"),
    ("POST", "/api/v2/files/move"),
    ("POST", "/api/v2/files/delete"),
    ("GET", "/api/v2/files/download"),
    ("POST", "/api/v2/files/mkdir"),
    ("GET", "/api/v2/files/stat"),
    ("POST", "/api/v2/catalog-sync/jobs/run"),
    ("GET", "/api/v2/catalog-sync/jobs"),
    ("GET", "/api/v2/catalog-sync/jobs/*"),
    ("POST", "/api/v2/catalog-sync/schedules"),
    ("GET", "/api/v2/catalog-sync/schedules"),
    ("DELETE", "/api/v2/catalog-sync/schedules/*"),
    ("GET", "/api/v2/ebins/catalog"),
    ("POST", "/api/v2/ebins/rescan"),
    ("POST", "/api/v2/ebins/validate"),
    ("POST", "/api/v2/ebins/load"),
    ("POST", "/api/v2/ebins/unload"),
}

RE_ROUTE_DEF = re.compile(
    r"httpd_uri_t\s+(?P<name>\w+)\s*=\s*\{"
    r"(?P<body>.*?)"
    r"\};",
    re.S,
)
RE_URI = re.compile(r"\.uri\s*=\s*\"(?P<uri>[^\"]+)\"")
RE_METHOD = re.compile(r"\.method\s*=\s*(?P<method>HTTP_[A-Z]+)")
RE_HANDLER = re.compile(r"\.handler\s*=\s*(?P<handler>\w+)")
RE_REGISTER = re.compile(r"httpd_register_uri_handler\(server_handle,\s*&(?P<name>\w+)\)")
RE_FUNC_DEF = re.compile(r"\b(?:static\s+)?(?:esp_err_t|void|bool|int|char\s*\*)\s+(?P<name>[A-Za-z_]\w*)\s*\(")
RE_ROUTE_CALL = re.compile(r"(?P<name>esptari_web_[a-z_]+_register_routes)\(server_handle\);")
RE_STRCMP_URI = re.compile(r"strcmp\(req->uri,\s*\"([^\"]+)\"\)")
RE_STRSTR_URI = re.compile(r"strstr\(req->uri,\s*\"([^\"]+)\"\)")


@dataclass(frozen=True)
class Route:
    method: str
    uri: str
    handler: str
    source: str


def parse_registered_routes(path: Path) -> list[Route]:
    content = path.read_text(encoding="utf-8")
    defs: dict[str, Route] = {}
    for match in RE_ROUTE_DEF.finditer(content):
        body = match.group("body")
        uri_match = RE_URI.search(body)
        method_match = RE_METHOD.search(body)
        handler_match = RE_HANDLER.search(body)
        if not uri_match or not method_match or not handler_match:
            continue
        defs[match.group("name")] = Route(
            method=method_match.group("method").replace("HTTP_", ""),
            uri=uri_match.group("uri"),
            handler=handler_match.group("handler"),
            source=path.name,
        )

    ordered: list[Route] = []
    for reg in RE_REGISTER.finditer(content):
        name = reg.group("name")
        if name in defs:
            ordered.append(defs[name])
    return ordered


def parse_symbols(c_files: list[Path]) -> set[str]:
    symbols: set[str] = set()
    for file_path in c_files:
        text = file_path.read_text(encoding="utf-8")
        for m in RE_FUNC_DEF.finditer(text):
            symbols.add(m.group("name"))
    return symbols


def fail(errors: list[str], msg: str) -> None:
    errors.append(msg)


def main() -> int:
    if not WEB_DIR.exists():
        print(f"ERROR: missing directory {WEB_DIR}")
        return 2

    c_files = sorted(WEB_DIR.glob("*.c"))
    all_symbols = parse_symbols(c_files)

    routes: list[Route] = []
    for c_file in c_files:
        routes.extend(parse_registered_routes(c_file))

    errors: list[str] = []

    if not routes:
        fail(errors, "No registered routes were discovered.")

    for route in routes:
        if route.handler not in all_symbols:
            fail(errors, f"Handler symbol not found: {route.handler} ({route.method} {route.uri} in {route.source})")

    routes_by_key: dict[tuple[str, str], list[Route]] = defaultdict(list)
    for route in routes:
        routes_by_key[(route.method, route.uri)].append(route)

    for key, key_routes in sorted(routes_by_key.items()):
        handlers = {r.handler for r in key_routes}
        if len(handlers) > 1:
            fail(errors, f"Conflicting handlers for {key[0]} {key[1]}: {sorted(handlers)}")

    actual = set(routes_by_key.keys())
    missing = sorted(EXPECTED_ROUTES - actual)
    extra = sorted(actual - EXPECTED_ROUTES)

    if missing:
        fail(errors, "Missing expected routes:")
        for method, uri in missing:
            fail(errors, f"  - {method} {uri}")

    if extra:
        fail(errors, "Unexpected routes not tracked by verifier:")
        for method, uri in extra:
            fail(errors, f"  - {method} {uri}")

    mappings_path = WEB_DIR / "esptari_web_mappings.c"
    mappings_routes = parse_registered_routes(mappings_path)
    mappings_order = [(r.method, r.uri) for r in mappings_routes]
    try:
        idx_active = mappings_order.index(("GET", "/api/v2/input/mappings/active"))
        idx_wild_get = mappings_order.index(("GET", "/api/v2/input/mappings/*"))
        if idx_active > idx_wild_get:
            fail(errors, "Route ordering error: GET /api/v2/input/mappings/active must register before GET wildcard mapping route.")
    except ValueError:
        fail(errors, "Mappings route ordering check could not locate expected active/wildcard routes.")

    catalogs_path = WEB_DIR / "esptari_web_catalog.c"
    catalogs_routes = parse_registered_routes(catalogs_path)
    cat_order = [(r.method, r.uri) for r in catalogs_routes]
    try:
        idx_list = cat_order.index(("GET", "/api/v2/catalogs/list"))
        idx_wild_get = cat_order.index(("GET", "/api/v2/catalogs/*"))
        if idx_list > idx_wild_get:
            fail(errors, "Route ordering error: GET /api/v2/catalogs/list must register before GET /api/v2/catalogs/*.")
    except ValueError:
        fail(errors, "Catalog route ordering check could not locate expected list/wildcard routes.")

    catalog_read_text = (WEB_DIR / "esptari_web_catalog_read.c").read_text(encoding="utf-8")
    catalog_post_text = (WEB_DIR / "esptari_web_catalog_write.c").read_text(encoding="utf-8")

    read_fragments = set(RE_STRSTR_URI.findall(catalog_read_text)) | set(RE_STRCMP_URI.findall(catalog_read_text))
    required_read = {
        "/api/v2/catalogs/list",
        "/missing-report",
        "/entries/",
        "/entries",
    }
    missing_read = sorted(required_read - read_fragments)
    if missing_read:
        fail(errors, "Catalog GET router missing dispatch fragments:")
        for frag in missing_read:
            fail(errors, f"  - {frag}")

    post_fragments = set(RE_STRSTR_URI.findall(catalog_post_text))
    required_post = {
        "/download-entry",
        "/download-missing",
        "/probe-links",
        "/mark-dead",
        "/rescan-local",
    }
    missing_post = sorted(required_post - post_fragments)
    if missing_post:
        fail(errors, "Catalog POST router missing dispatch fragments:")
        for frag in missing_post:
            fail(errors, f"  - {frag}")

    web_init_text = (WEB_DIR / "esptari_web.c").read_text(encoding="utf-8")
    route_call_names = set(RE_ROUTE_CALL.findall(web_init_text))
    required_registrars = {
        "esptari_web_core_status_register_routes",
        "esptari_web_lifecycle_register_routes",
        "esptari_web_media_register_routes",
        "esptari_web_input_register_routes",
        "esptari_web_mappings_register_routes",
        "esptari_web_stream_register_routes",
        "esptari_web_debug_register_routes",
        "esptari_web_metrics_register_routes",
        "esptari_web_persistence_register_routes",
        "esptari_web_snapshot_register_routes",
        "esptari_web_catalog_register_routes",
        "esptari_web_files_register_routes",
        "esptari_web_catalog_sync_register_routes",
    }
    missing_registrars = sorted(required_registrars - route_call_names)
    if missing_registrars:
        fail(errors, "Top-level web init missing registrar calls:")
        for name in missing_registrars:
            fail(errors, f"  - {name}")

    if errors:
        print("API V2 route verification FAILED")
        for error in errors:
            print(error)
        return 1

    print("API V2 route verification PASSED")
    print(f"Validated routes: {len(actual)}")
    for method, uri in sorted(actual):
        print(f"  {method:<6} {uri}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
