#!/usr/bin/env python3
import argparse
import json
import re
import sys
from pathlib import Path
from typing import List


SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+$")
MODULE_RE = re.compile(r"^st\.[a-z0-9_]+(?:\.[a-z0-9_]+)+$")


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def require_fields(obj: dict, fields: List[str], label: str) -> None:
    missing = [key for key in fields if key not in obj]
    if missing:
        raise ValueError(f"{label}: missing required fields: {', '.join(missing)}")


def validate_contract(contract: dict) -> None:
    require_fields(contract, ["freeze_id", "freeze_version", "machine", "output_layout", "module_naming", "metadata_schema"], "contract")
    if contract["freeze_id"] != "EBIN-202":
        raise ValueError("contract: freeze_id must be EBIN-202")
    if int(contract["freeze_version"]) != 1:
        raise ValueError("contract: freeze_version must be 1")
    if contract["machine"] != "atari_st":
        raise ValueError("contract: machine must be atari_st")

    output_layout = contract["output_layout"]
    require_fields(output_layout, ["root_rel", "component_dirs", "file_patterns"], "output_layout")
    component_dirs = output_layout["component_dirs"]
    for component in ["cpu", "chipset", "io", "storage", "audio_gpio", "machine_profile", "packages"]:
        if component not in component_dirs:
            raise ValueError(f"output_layout.component_dirs: missing {component}")

    naming = contract["module_naming"]
    require_fields(naming, ["shared_modules", "machine_profile_modules", "component_by_module"], "module_naming")
    all_modules = naming["shared_modules"] + naming["machine_profile_modules"]
    if len(all_modules) != len(set(all_modules)):
        raise ValueError("module_naming: duplicate module entries")

    for module in all_modules:
        if not MODULE_RE.match(module):
            raise ValueError(f"module_naming: invalid module id {module}")
        if module not in naming["component_by_module"]:
            raise ValueError(f"module_naming.component_by_module: missing mapping for {module}")

    metadata = contract["metadata_schema"]
    for section in ["component_index", "component_manifest", "package_index", "package_manifest"]:
        if section not in metadata:
            raise ValueError(f"metadata_schema: missing section {section}")
        require_fields(metadata[section], ["schema", "required_fields"], f"metadata_schema.{section}")
        if "layout_version" not in metadata[section]["required_fields"]:
            raise ValueError(f"metadata_schema.{section}: required_fields must include layout_version")


def validate_component_metadata(root: Path, contract: dict) -> None:
    base = root / contract["output_layout"]["root_rel"]
    if not base.exists():
        raise ValueError(f"artifact root missing expected base path: {base}")

    component_dirs = contract["output_layout"]["component_dirs"]
    for component, rel in component_dirs.items():
        if component == "packages":
            continue
        path = base / rel
        if not path.exists():
            raise ValueError(f"missing component directory: {path}")

    machine_profile_dir = base / component_dirs["machine_profile"]
    index_path = machine_profile_dir / "index.json"
    manifest_path = machine_profile_dir / "manifest.json"
    if not index_path.exists() or not manifest_path.exists():
        raise ValueError("machine_profile metadata not present (index.json/manifest.json)")

    index = load_json(index_path)
    manifest = load_json(manifest_path)

    index_required = contract["metadata_schema"]["component_index"]["required_fields"]
    manifest_required = contract["metadata_schema"]["component_manifest"]["required_fields"]

    require_fields(index, index_required, "machine_profile/index.json")
    require_fields(manifest, manifest_required, "machine_profile/manifest.json")

    if index["machine"] != contract["machine"]:
        raise ValueError("machine_profile/index.json: machine mismatch")
    if manifest["machine"] != contract["machine"]:
        raise ValueError("machine_profile/manifest.json: machine mismatch")

    module_id = index["module_id"]
    if module_id not in contract["module_naming"]["machine_profile_modules"]:
        raise ValueError(f"machine_profile/index.json: module_id not frozen: {module_id}")

    if not isinstance(index["versions"], list) or not index["versions"]:
        raise ValueError("machine_profile/index.json: versions must be non-empty list")
    for version in index["versions"]:
        if not isinstance(version, str) or not SEMVER_RE.match(version):
            raise ValueError(f"machine_profile/index.json: invalid semver {version}")

    winner = index["winner"]
    if winner not in index["versions"]:
        raise ValueError("machine_profile/index.json: winner must exist in versions")

    winner_selector = f"{module_id}@{winner}"
    if manifest["winner"] != winner_selector:
        raise ValueError(
            "machine_profile/manifest.json: winner mismatch "
            f"(expected {winner_selector}, got {manifest['winner']})"
        )

    expected_component = contract["module_naming"]["component_by_module"][module_id]
    if index["component"] != expected_component or manifest["component"] != expected_component:
        raise ValueError("machine_profile metadata component mismatch")

    for filename in index["files"]:
        if not filename.startswith(f"{module_id}-") or not filename.endswith(".ebin"):
            raise ValueError(f"machine_profile/index.json: invalid file entry {filename}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate EBIN-202 ST520/ST1040 layout/schema freeze contract")
    parser.add_argument(
        "--contract",
        default="docs/emu_engine_v2/reference_ebin_packages/st520_st1040_ebin_freeze_v1.json",
        help="Path to freeze contract JSON",
    )
    parser.add_argument(
        "--artifact-root",
        default="",
        help="Optional root directory containing ebins/atari_st tree to validate",
    )
    args = parser.parse_args()

    contract_path = Path(args.contract)
    contract = load_json(contract_path)
    validate_contract(contract)

    if args.artifact_root:
        validate_component_metadata(Path(args.artifact_root), contract)

    print("validation=pass contract=EBIN-202")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"validation=fail error={exc}", file=sys.stderr)
        raise SystemExit(1)