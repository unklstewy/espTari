#!/usr/bin/env python3
"""
Scraper for SidecarTridge Atari ST ROMs Database.
Downloads ROM images (.bin, .img, .rom, .stc) and organizes them by tag.
Generates a roms_catalog.json with full metadata.

Data source: http://roms.sidecartridge.com/roms.json
Files hosted at: http://roms.sidecartridge.com/

Usage:
    python3 scrape_roms.py [--dry-run] [--catalog-only]
"""

import json
import os
import sys
import time
import hashlib
import argparse
import requests
from pathlib import Path
from urllib.parse import urlparse, unquote, quote
from datetime import datetime, timezone

# Source URL
ROMS_JSON_URL = "http://roms.sidecartridge.com/roms.json"

# Output directory
OUTPUT_DIR = Path(__file__).parent / "roms"

# Download cache
CACHE_DIR = OUTPUT_DIR / ".roms_cache"

# Catalog output path
CATALOG_PATH = Path(__file__).parent / "roms_catalog.json"


def fetch_roms_json(session):
    """Fetch the roms.json database."""
    resp = session.get(ROMS_JSON_URL, timeout=15)
    resp.raise_for_status()
    return resp.json()


def classify_rom(rom):
    """Determine the primary folder for a ROM based on its tags."""
    tags = [t.lower() for t in rom.get("tags", [])]
    name = rom.get("name", "").lower()

    # Priority order for classification
    if "os" in tags:
        return "OS"
    if "diagnostic" in tags or "test" in tags:
        return "Diagnostic"
    if any(t in tags for t in ["arcade", "game", "puzzle", "platformer",
                                "racing", "shooter", "action", "adventure",
                                "simulation", "space"]):
        return "Games"
    if any(t in tags for t in ["tool", "ripper", "disk", "utility",
                                "management", "desktop", "debugging"]):
        return "Tools"
    if any(t in tags for t in ["programming", "basic", "coding"]):
        return "Programming"
    if any(t in tags for t in ["emulator", "macintosh", "terminal"]):
        return "Emulators"
    if "homebrew" in tags:
        return "Homebrew"
    if any(t in tags for t in ["demo"]):
        return "Demos"
    if "system" in tags or "sys" in tags:
        return "System"

    return "Other"


def build_catalog(roms_data):
    """Build the roms_catalog.json structure."""
    all_tags = sorted({t for rom in roms_data for t in rom.get("tags", [])})

    catalog = {
        "metadata": {
            "source": "SidecarTridge Atari ST ROMs Database",
            "source_url": ROMS_JSON_URL,
            "repo_url": "https://github.com/sidecartridge/atarist-roms-database",
            "scraped_at": datetime.now(timezone.utc).isoformat(),
            "total_roms": len(roms_data),
            "all_tags": all_tags,
        },
        "roms": [],
    }

    for rom in sorted(roms_data, key=lambda r: r["name"].lower()):
        url = rom["url"]
        filename = unquote(urlparse(url).path.split('/')[-1])
        folder = classify_rom(rom)
        local_path = f"{folder}/{filename}"

        catalog["roms"].append({
            "name": rom["name"],
            "description": rom.get("description", ""),
            "tags": rom.get("tags", []),
            "size_kb": rom.get("size_kb"),
            "url": url,
            "filename": filename,
            "folder": folder,
            "local_path": local_path,
        })

    return catalog


def download_file(url, dest_path, session, retries=3):
    """Download a file with retry logic."""
    for attempt in range(retries):
        try:
            resp = session.get(url, timeout=30, stream=True)
            if resp.status_code == 200:
                dest_path.parent.mkdir(parents=True, exist_ok=True)
                with open(dest_path, 'wb') as f:
                    for chunk in resp.iter_content(chunk_size=8192):
                        f.write(chunk)
                return True
            elif resp.status_code == 404:
                print(f"  [404] Not found: {url}")
                return False
            else:
                print(f"  [HTTP {resp.status_code}] {url} (attempt {attempt+1}/{retries})")
        except requests.exceptions.RequestException as e:
            print(f"  [Error] {url}: {e} (attempt {attempt+1}/{retries})")

        if attempt < retries - 1:
            time.sleep(2 ** attempt)

    return False


def get_cache_path(url):
    """Get the cache file path for a URL."""
    url_hash = hashlib.md5(url.encode()).hexdigest()[:12]
    filename = unquote(urlparse(url).path.split('/')[-1])
    return CACHE_DIR / f"{url_hash}_{filename}"


def main():
    parser = argparse.ArgumentParser(
        description="Download and organize SidecarTridge Atari ST ROMs"
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="Parse and show what would be downloaded without downloading")
    parser.add_argument("--catalog-only", action="store_true",
                        help="Only generate the catalog JSON, don't download files")
    args = parser.parse_args()

    session = requests.Session()
    session.headers.update({
        "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) ROMScraper/1.0"
    })

    print(f"Fetching ROMs database from {ROMS_JSON_URL}...")
    roms_data = fetch_roms_json(session)
    print(f"Found {len(roms_data)} ROMs")

    # Generate catalog
    catalog = build_catalog(roms_data)
    with open(CATALOG_PATH, 'w', encoding='utf-8') as f:
        json.dump(catalog, f, indent=2, ensure_ascii=False)
    print(f"Wrote catalog to {CATALOG_PATH}")

    # Folder summary
    folder_counts = {}
    for rom in catalog["roms"]:
        folder_counts[rom["folder"]] = folder_counts.get(rom["folder"], 0) + 1
    print(f"\nFolders:")
    for folder in sorted(folder_counts.keys()):
        print(f"  {folder}/: {folder_counts[folder]} ROMs")

    print(f"\nTags: {', '.join(catalog['metadata']['all_tags'])}")

    if args.catalog_only:
        print("\n--catalog-only mode, skipping downloads.")
        return

    if args.dry_run:
        print(f"\n=== DRY RUN ===")
        print(f"Would download {len(roms_data)} ROMs to {OUTPUT_DIR}")
        for rom in catalog["roms"]:
            print(f"  {rom['folder']}/{rom['filename']} ({rom['size_kb']}KB)")
        return

    # Download files organized by tag folder
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)

    downloaded = 0
    skipped = 0
    failed = 0
    total = len(catalog["roms"])

    print(f"\nDownloading {total} ROM files...\n")

    for i, rom in enumerate(catalog["roms"], 1):
        cache_path = get_cache_path(rom["url"])
        target_path = OUTPUT_DIR / rom["folder"] / rom["filename"]

        if target_path.exists():
            skipped += 1
            continue

        if not cache_path.exists():
            print(f"  [{i}/{total}] Downloading: {rom['name']} ({rom['filename']})")
            success = download_file(rom["url"], cache_path, session)
            if not success:
                failed += 1
                continue
            downloaded += 1
            time.sleep(0.3)
        else:
            skipped += 1

        # Copy to target
        target_path.parent.mkdir(parents=True, exist_ok=True)
        import shutil
        shutil.copy2(cache_path, target_path)

    print(f"\n{'='*60}")
    print(f"Download complete!")
    print(f"  Downloaded: {downloaded}")
    print(f"  Skipped: {skipped}")
    print(f"  Failed: {failed}")
    print(f"\nFiles organized in: {OUTPUT_DIR}")
    for folder in sorted(folder_counts.keys()):
        folder_dir = OUTPUT_DIR / folder
        if folder_dir.exists():
            file_count = sum(1 for _ in folder_dir.iterdir())
            print(f"  {folder}/ ({file_count} files)")


if __name__ == "__main__":
    main()
