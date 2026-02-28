#!/usr/bin/env python3
"""
Scraper for SidecarTridge Atari ST Public Floppy Database.
Downloads floppy disk images (.ST) and organizes them by category.
Generates a floppy_catalog.json with full metadata.

Data source: http://ataristdb.sidecartridge.com/db/{letter}.csv
Files hosted at: http://ataristdb.sidecartridge.com/{file_path}

Usage:
    python3 scrape_floppies.py [--dry-run] [--catalog-only]
"""

import csv
import io
import json
import os
import re
import string
import sys
import time
import hashlib
import argparse
import requests
from pathlib import Path
from urllib.parse import quote, unquote
from datetime import datetime, timezone

# Base URLs
DB_BASE_URL = "http://ataristdb.sidecartridge.com/db"
FILE_BASE_URL = "http://ataristdb.sidecartridge.com"

# Output directory (current directory)
OUTPUT_DIR = Path(__file__).parent / "floppies"

# Download cache
CACHE_DIR = OUTPUT_DIR / ".floppy_cache"

# Catalog output path
CATALOG_PATH = Path(__file__).parent / "floppy_catalog.json"

# CSV shards to fetch: a-z, _, 0-9
SHARDS = list(string.ascii_lowercase) + ["_"] + list(string.digits)


def fetch_csv_shard(shard_name, session):
    """Fetch a single CSV shard from the floppy database."""
    url = f"{DB_BASE_URL}/{shard_name}.csv"
    try:
        resp = session.get(url, timeout=15)
        if resp.status_code == 200:
            return resp.text
        elif resp.status_code == 404 or resp.status_code == 403:
            return None  # Shard doesn't exist
        else:
            print(f"  [HTTP {resp.status_code}] {url}")
            return None
    except requests.exceptions.RequestException as e:
        print(f"  [Error] {url}: {e}")
        return None


def parse_csv_shard(csv_text, shard_name):
    """Parse a semicolon-delimited CSV shard into a list of entries."""
    entries = []
    reader = csv.reader(io.StringIO(csv_text), delimiter=';')
    for row in reader:
        if len(row) < 6:
            continue
        title = row[0].strip()
        # value1 is typically "0"
        timestamp_str = row[2].strip()
        # value3 is typically empty
        category = row[4].strip()
        file_path = row[5].strip()

        # Parse timestamp
        timestamp = None
        if timestamp_str:
            try:
                ts = int(timestamp_str)
                if ts > 0:
                    timestamp = datetime.fromtimestamp(ts, tz=timezone.utc).isoformat()
            except (ValueError, OSError):
                pass

        # Handle entries where file_path is a full URL instead of relative
        if file_path.startswith("http://") or file_path.startswith("https://"):
            download_url = file_path
            # Extract relative path from URL
            # e.g. http://ataristdb.sidecartridge.com/MISC/FOO.ST -> MISC/FOO.ST
            try:
                from urllib.parse import urlparse as _urlparse
                parsed = _urlparse(file_path)
                file_path = parsed.path.lstrip('/')
            except Exception:
                pass
        else:
            download_url = f"{FILE_BASE_URL}/{quote(file_path, safe='/')}"

        # Extract filename from path
        filename = file_path.split('/')[-1] if file_path else None
        # Extract collection prefix
        collection = file_path.split('/')[0] if '/' in file_path else None

        if not title or not file_path:
            continue

        entries.append({
            "title": title,
            "category": category if category else "UNKNOWN",
            "collection": collection,
            "file_path": file_path,
            "filename": filename,
            "timestamp": timestamp,
            "shard": shard_name,
            "download_url": download_url,
        })

    return entries


def build_catalog(all_entries):
    """Build the floppy_catalog.json structure."""
    categories = sorted({e["category"] for e in all_entries})
    collections = sorted({e["collection"] for e in all_entries if e["collection"]})

    catalog = {
        "metadata": {
            "source": "SidecarTridge Atari ST Public Floppy Database",
            "source_url": "http://ataristdb.sidecartridge.com/db/",
            "repo_url": "https://github.com/sidecartridge/atarist-public-floppy-db",
            "scraped_at": datetime.now(timezone.utc).isoformat(),
            "total_entries": len(all_entries),
            "categories": categories,
            "collections": collections,
        },
        "entries": [],
    }

    for entry in sorted(all_entries, key=lambda e: e["title"].lower()):
        local_path = f"{entry['category']}/{entry['filename']}"
        catalog["entries"].append({
            "title": entry["title"],
            "category": entry["category"],
            "collection": entry["collection"],
            "filename": entry["filename"],
            "file_path": entry["file_path"],
            "download_url": entry["download_url"],
            "timestamp": entry["timestamp"],
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


def get_cache_path(entry):
    """Get the cache file path for an entry."""
    url_hash = hashlib.md5(entry["download_url"].encode()).hexdigest()[:12]
    return CACHE_DIR / f"{url_hash}_{entry['filename']}"


def main():
    parser = argparse.ArgumentParser(
        description="Download and organize SidecarTridge Atari ST floppy images"
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="Parse and show what would be downloaded without downloading")
    parser.add_argument("--catalog-only", action="store_true",
                        help="Only generate the catalog JSON, don't download files")
    args = parser.parse_args()

    session = requests.Session()
    session.headers.update({
        "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) FloppyScraper/1.0"
    })

    # Fetch all CSV shards
    print(f"Fetching floppy database CSV shards...")
    all_entries = []
    for shard in SHARDS:
        csv_text = fetch_csv_shard(shard, session)
        if csv_text:
            entries = parse_csv_shard(csv_text, shard)
            if entries:
                print(f"  {shard}.csv: {len(entries)} entries")
                all_entries.extend(entries)
        time.sleep(0.2)  # Be polite

    print(f"\nTotal floppy entries: {len(all_entries)}")

    # Deduplicate by file_path (some entries may appear in _ shard and letter shard)
    seen = {}
    unique_entries = []
    for entry in all_entries:
        key = entry["file_path"]
        if key not in seen:
            seen[key] = True
            unique_entries.append(entry)

    print(f"Unique entries (after dedup): {len(unique_entries)}")

    # Generate catalog
    catalog = build_catalog(unique_entries)
    with open(CATALOG_PATH, 'w', encoding='utf-8') as f:
        json.dump(catalog, f, indent=2, ensure_ascii=False)
    print(f"Wrote catalog to {CATALOG_PATH}")

    # Category summary
    cat_counts = {}
    for e in unique_entries:
        cat_counts[e["category"]] = cat_counts.get(e["category"], 0) + 1
    print(f"\nCategories:")
    for cat in sorted(cat_counts.keys()):
        print(f"  {cat}: {cat_counts[cat]} files")

    if args.catalog_only:
        print("\n--catalog-only mode, skipping downloads.")
        return

    if args.dry_run:
        print(f"\n=== DRY RUN ===")
        print(f"Would download {len(unique_entries)} floppy images to {OUTPUT_DIR}")
        return

    # Download files organized by category
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)

    downloaded = 0
    skipped = 0
    failed = 0
    total = len(unique_entries)

    print(f"\nDownloading {total} floppy images...\n")

    for i, entry in enumerate(unique_entries, 1):
        cache_path = get_cache_path(entry)
        target_path = OUTPUT_DIR / entry["category"] / entry["filename"]

        if target_path.exists():
            skipped += 1
            if i % 100 == 0:
                print(f"  Progress: {i}/{total} (downloaded: {downloaded}, skipped: {skipped}, failed: {failed})")
            continue

        # Download to cache if not cached
        if not cache_path.exists():
            print(f"  [{i}/{total}] Downloading: {entry['filename']}")
            success = download_file(entry["download_url"], cache_path, session)
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

        if i % 50 == 0:
            print(f"  Progress: {i}/{total} (downloaded: {downloaded}, skipped: {skipped}, failed: {failed})")

    print(f"\n{'='*60}")
    print(f"Download complete!")
    print(f"  Downloaded: {downloaded}")
    print(f"  Skipped: {skipped}")
    print(f"  Failed: {failed}")
    print(f"\nFiles organized in: {OUTPUT_DIR}")
    for cat in sorted(cat_counts.keys()):
        cat_dir = OUTPUT_DIR / cat
        if cat_dir.exists():
            file_count = sum(1 for _ in cat_dir.iterdir())
            print(f"  {cat}/ ({file_count} files)")


if __name__ == "__main__":
    main()
