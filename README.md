# espTari Emulation Engine v2

espTari is an Atari ST-first emulation runtime for ESP32-P4, centered on a contract-driven V2 backend API and deterministic inspection surfaces.

This repository now treats V2 as the primary runtime line:

- Control and lifecycle APIs under /api/v2
- Component-level inspection and conformance endpoints
- SD-card-backed media/module model
- Task/evidence driven delivery tracked in TRACKING

## Current V2 Focus

- Machine target: Atari ST baseline profile
- Runtime target: Waveshare ESP32-P4-NANO
- SDK target: ESP-IDF 5.5.2
- Primary contract document: docs/EMU_ENGINE_V2_API_SPEC.md
- Primary implementation plan: docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md

## Architecture Summary

V2 is organized into five execution planes:

1. Control plane: session lifecycle, profile selection, orchestration
2. Emulation plane: deterministic stepping, scheduling, component coordination
3. I/O and media plane: SD-card assets, attach/detach flows, host input ingress
4. Observability plane: registers/bus/memory/timing inspection APIs
5. Streaming plane: video/audio plus metadata telemetry channels

API style:

- Base path: /api/v2
- Transport: HTTP JSON and WebSocket stream surfaces
- Envelope and error taxonomy defined by V2 spec

## Repository Map (V2-Relevant)

- components/esptari_web: V2 HTTP route registration and handlers
- components/esptari_core: runtime/session state and core orchestration hooks
- docs/EMU_ENGINE_V2_API_SPEC.md: normative API contract
- docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md: architecture and sequencing plan
- docs/emu_engine_v2: hardware and behavior source spec chapters
- TRACKING: backlog, sprint, acceptance, evidence, and delivery governance
- tools: smoke scripts, SD card prep, EBIN tooling, MCP tracking server
- frontend: browser client workspace (Vite/Vue app scaffolding)

## Build and Flash (Firmware)

From repository root:

```bash
idf.py build
idf.py flash
idf.py monitor
```

If you need an explicit serial target:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

## Frontend Workspace

The frontend workspace lives under frontend.

```bash
cd frontend
npm install
npm run build
```

## Development Workflow

For V2 contract work:

1. Update or confirm contract language in docs/EMU_ENGINE_V2_API_SPEC.md
2. Implement/adjust handlers under components/esptari_web
3. Build and flash firmware
4. Run task smoke validation scripts under tools
5. Store evidence in captures and record acceptance in TRACKING/tracking.db

## Tracking and Evidence

Use TRACKING as the single execution ledger for:

- Task status and dependencies
- Acceptance decisions and artifact links
- Sprint/Kanban governance and release notes

Entry points:

- TRACKING/README.md
- TRACKING/BACKLOG.md
- TRACKING/KANBAN_BOARD.md
- TRACKING/ACCEPTANCE_LOG.md
- TRACKING/UMBRELLA_CLOSURE_REPORT_2026-03-03.md

## MCP Tooling (Workspace)

This workspace includes an MCP server for tracking/docs navigation:

- tools/mcp_tracking_docs_server_node/server.mjs
- .vscode/mcp.json

## Branding Assets

- Source branding: assets/logo
- Frontend icons: frontend/public

## Status

V2 umbrella backlog closure has been completed and captured in tracking artifacts. Ongoing work should continue as contract-aligned incremental slices with smoke evidence and acceptance logging.
