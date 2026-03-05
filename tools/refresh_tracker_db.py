#!/usr/bin/env python3

import json
import os
import sqlite3
from datetime import datetime, timezone
from pathlib import Path


def source_category(workspace: Path, file_path: Path) -> str:
    try:
        rel = file_path.relative_to(workspace)
        rel_str = rel.as_posix()
        if rel_str.startswith("tools/ebin_builder/"):
            return "ebin_builder_tool"
        if rel_str.startswith("components/") or rel_str.startswith("main/") or rel_str.startswith("tools/"):
            return "project"
        if rel_str.startswith("managed_components/"):
            return "managed_component"
        if rel_str.startswith("build/"):
            return "generated_build"
        return "workspace_other"
    except ValueError:
        path_str = file_path.as_posix()
        if "/esp-idf/components/" in path_str:
            return "esp_idf"
        return "external"


def component_name(workspace: Path, file_path: Path) -> str:
    try:
        rel = file_path.relative_to(workspace)
        parts = rel.parts
        if len(parts) >= 2 and parts[0] in {"components", "managed_components"}:
            return parts[1]
        if parts:
            return parts[0]
    except ValueError:
        path_str = file_path.as_posix()
        marker = "/esp-idf/components/"
        if marker in path_str:
            tail = path_str.split(marker, 1)[1]
            return tail.split("/", 1)[0]
    return "unknown"


def collect_ebin_builder_sources(workspace: Path) -> dict[str, dict[str, str | None]]:
    ebin_root = workspace / "tools" / "ebin_builder"
    if not ebin_root.exists():
        return {}

    sources: dict[str, dict[str, str | None]] = {}
    for ext in ("*.c", "*.py", "*.sh"):
        for source_path in ebin_root.rglob(ext):
            if not source_path.is_file():
                continue
            sources[str(source_path.resolve())] = {
                "output": None,
                "directory": str(source_path.parent.resolve()),
            }
    return sources


def ensure_schema(cur: sqlite3.Cursor) -> None:
    cur.executescript(
        """
        CREATE TABLE IF NOT EXISTS CodeComments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            fileName TEXT NOT NULL,
            filePath TEXT NOT NULL UNIQUE,
            fileSize INTEGER,
            shortDesc TEXT,
            primaryFunction TEXT,
            longDesc TEXT,
            commentsCompleted INTEGER NOT NULL DEFAULT 0,
            completedTimeStamp TEXT,
            sourceCategory TEXT,
            componentName TEXT,
            compileOutput TEXT,
            compileDirectory TEXT,
            owner TEXT,
            reviewStatus TEXT NOT NULL DEFAULT 'not_started',
            priority INTEGER NOT NULL DEFAULT 3,
            createdAt TEXT NOT NULL,
            updatedAt TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS SwaggerCompletionChecklist (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            fileName TEXT NOT NULL,
            filePath TEXT NOT NULL UNIQUE,
            fileSize INTEGER,
            shortDesc TEXT,
            primaryFunction TEXT,
            longDesc TEXT,
            commentsCompleted INTEGER NOT NULL DEFAULT 0,
            completedTimeStamp TEXT,
            sourceCategory TEXT,
            componentName TEXT,
            compileOutput TEXT,
            compileDirectory TEXT,
            owner TEXT,
            reviewStatus TEXT NOT NULL DEFAULT 'not_started',
            swaggerStatus TEXT NOT NULL DEFAULT 'not_started',
            endpointCount INTEGER,
            coveredEndpointCount INTEGER,
            createdAt TEXT NOT NULL,
            updatedAt TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_codecomments_sourceCategory ON CodeComments(sourceCategory);
        CREATE INDEX IF NOT EXISTS idx_codecomments_reviewStatus ON CodeComments(reviewStatus);
        CREATE INDEX IF NOT EXISTS idx_swagger_sourceCategory ON SwaggerCompletionChecklist(sourceCategory);
        CREATE INDEX IF NOT EXISTS idx_swagger_swaggerStatus ON SwaggerCompletionChecklist(swaggerStatus);
        """
    )


def main() -> int:
    workspace = Path(__file__).resolve().parents[1]
    compile_commands_path = workspace / "build" / "compile_commands.json"
    db_path = workspace / "codeCommentAndSwaggerTracker.db"

    if not compile_commands_path.exists():
        raise FileNotFoundError(f"Missing compile database: {compile_commands_path}")

    with compile_commands_path.open("r", encoding="utf-8") as handle:
        entries = json.load(handle)

    compiled_c = {}
    for entry in entries:
        file_value = entry.get("file", "")
        if not file_value.endswith(".c"):
            continue
        source_path = Path(file_value).resolve()
        compiled_c[str(source_path)] = {
            "output": entry.get("output", ""),
            "directory": entry.get("directory", ""),
        }

    ebin_sources = collect_ebin_builder_sources(workspace)
    tracked_sources = {**compiled_c, **ebin_sources}

    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    ensure_schema(cur)

    current_paths = set(tracked_sources.keys())

    for table_name in ("CodeComments", "SwaggerCompletionChecklist"):
        cur.execute(f"DELETE FROM {table_name} WHERE filePath NOT IN ({','.join(['?'] * len(current_paths))})" if current_paths else f"DELETE FROM {table_name}", tuple(current_paths) if current_paths else ())

    for file_path_str, meta in sorted(tracked_sources.items(), key=lambda item: item[0].lower()):
        source_path = Path(file_path_str)
        file_name = source_path.name
        file_size = os.path.getsize(source_path) if source_path.exists() else None
        src_category = source_category(workspace, source_path)
        comp_name = component_name(workspace, source_path)

        cur.execute(
            """
            INSERT INTO CodeComments (
                fileName, filePath, fileSize, shortDesc, primaryFunction, longDesc,
                commentsCompleted, completedTimeStamp,
                sourceCategory, componentName, compileOutput, compileDirectory,
                owner, reviewStatus, priority, createdAt, updatedAt
            ) VALUES (?, ?, ?, NULL, NULL, NULL, 0, NULL, ?, ?, ?, ?, NULL, 'not_started', 3, ?, ?)
            ON CONFLICT(filePath) DO UPDATE SET
                fileName=excluded.fileName,
                fileSize=excluded.fileSize,
                sourceCategory=excluded.sourceCategory,
                componentName=excluded.componentName,
                compileOutput=excluded.compileOutput,
                compileDirectory=excluded.compileDirectory,
                updatedAt=excluded.updatedAt
            """,
            (
                file_name,
                file_path_str,
                file_size,
                src_category,
                comp_name,
                meta.get("output"),
                meta.get("directory"),
                now,
                now,
            ),
        )

        cur.execute(
            """
            INSERT INTO SwaggerCompletionChecklist (
                fileName, filePath, fileSize, shortDesc, primaryFunction, longDesc,
                commentsCompleted, completedTimeStamp,
                sourceCategory, componentName, compileOutput, compileDirectory,
                owner, reviewStatus, swaggerStatus, endpointCount, coveredEndpointCount,
                createdAt, updatedAt
            ) VALUES (?, ?, ?, NULL, NULL, NULL, 0, NULL, ?, ?, ?, ?, NULL, 'not_started', 'not_started', NULL, NULL, ?, ?)
            ON CONFLICT(filePath) DO UPDATE SET
                fileName=excluded.fileName,
                fileSize=excluded.fileSize,
                sourceCategory=excluded.sourceCategory,
                componentName=excluded.componentName,
                compileOutput=excluded.compileOutput,
                compileDirectory=excluded.compileDirectory,
                updatedAt=excluded.updatedAt
            """,
            (
                file_name,
                file_path_str,
                file_size,
                src_category,
                comp_name,
                meta.get("output"),
                meta.get("directory"),
                now,
                now,
            ),
        )

    conn.commit()
    code_count = cur.execute("SELECT COUNT(*) FROM CodeComments").fetchone()[0]
    swagger_count = cur.execute("SELECT COUNT(*) FROM SwaggerCompletionChecklist").fetchone()[0]
    conn.close()

    print(f"db={db_path}")
    print(f"compiled_c_files={len(current_paths)}")
    print(f"idf_compiled_c_files={len(compiled_c)}")
    print(f"ebin_builder_source_files={len(ebin_sources)}")
    print(f"tracked_source_files={len(current_paths)}")
    print(f"CodeComments={code_count}")
    print(f"SwaggerCompletionChecklist={swagger_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
