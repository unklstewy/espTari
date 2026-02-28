#!/usr/bin/env python3
"""
Scraper for Atari TOS images from avtandil33.pythonanywhere.com/tose
Downloads all TOS ROM images and organizes them by Atari computer model.
Also generates a tos_catalog.json with full metadata for each TOS version and file.

Usage:
    python3 scrape_tos.py [--dry-run]

Files are organized into folders by Atari model. TOS versions that support
multiple machines are symlinked to avoid duplicate downloads.
"""

import json
import os
import re
import sys
import time
import hashlib
import argparse
import requests
from pathlib import Path
from urllib.parse import urlparse, unquote
from datetime import datetime, timezone

# Base URL for the TOS images page
PAGE_URL = "https://avtandil33.pythonanywhere.com/tose"

# Output directory (current directory)
OUTPUT_DIR = Path(__file__).parent

# Download cache to avoid re-downloading
CACHE_DIR = OUTPUT_DIR / ".tos_cache"

# Catalog output path
CATALOG_PATH = OUTPUT_DIR / "tos_catalog.json"


def normalize_machine(machine_str):
    """Extract and normalize individual machine names from a machines string."""
    machines = set()
    text = machine_str.lower().strip()

    if not text or text == "&nbsp;" or text == "nbsp":
        return machines

    # Use regex word-boundary matching to avoid substring issues
    # (e.g., "520ste" should NOT match "520st")
    # Order matters: check longer/more-specific patterns first
    patterns = [
        (r'\b520ste\b', '520STE'),
        (r'\b1040ste\b', '1040STE'),
        (r'\bmega\s*ste\b', 'Mega_STE'),
        (r'\b520st\b(?!e)', '520ST'),
        (r'\b1040st\b(?!e)', '1040ST'),
        (r'\bmega\s*[24]\b', 'Mega_ST'),
        (r'\bstacy\b', 'Stacy'),
        (r'\bst\s*book\b', 'ST_Book'),
        (r'\btt\s*030\b', 'TT030'),
        (r'\bfalcon\s*030\b', 'Falcon_030'),
    ]

    for pattern, normalized in patterns:
        if re.search(pattern, text):
            machines.add(normalized)

    # Special: "520ST/1040ST (with hardware modifications)" for TOS 2.06
    if '520st/1040st' in text:
        machines.add('520ST')
        machines.add('1040ST')

    # If "sparrow" or "fx-1" found
    if "sparrow" in text or "fx-1" in text:
        machines.add("Sparrow_FX1")

    # If "tex" in text and "giga" in text
    if "tex" in text and "giga" in text:
        machines.add("TEX_GIGA_ST")
    elif "tex" in text and "giga" not in text and "texa" not in text:
        if "custom" in text or "tos 1.7" in text:
            machines.add("TEX_GIGA_ST")

    return machines


def extract_field(section_html, field_name):
    """Extract a metadata field value from a TOS section's HTML."""
    pattern = rf'<b>{re.escape(field_name)}</b>.*?<td[^>]*>(.*?)</td>'
    match = re.search(pattern, section_html, re.DOTALL)
    if match:
        text = re.sub(r'<[^>]+>', '', match.group(1)).strip()
        # Clean up &nbsp; and excessive whitespace
        text = text.replace('&nbsp;', '').replace('&quot;', '"')
        text = re.sub(r'\s+', ' ', text).strip()
        return text if text else None
    return None


def extract_files_with_metadata(section_html):
    """Extract download links with their labels, tooltips, and image type (ROM vs disk)."""
    files = []

    # Find all labeled row sections: "<b>TOS X.XX Images:</b>" or "<b>TOS X.XX Disk Images:</b>"
    # and the links that follow them
    # We process links in order, tracking the current label row context
    label_pattern = re.compile(
        r'<b>([^<]*(?:Images?|installations?|Disk Images?)[^<]*)</b>'
    )
    link_pattern = re.compile(
        r'<a\s+href="(http://avtandil\.narod\.ru/tos/[^"]+\.zip)"'
        r'(?:\s+title="([^"]*?)")?[^>]*>(.*?)</a>',
        re.DOTALL
    )

    # Split into table rows to track context
    rows = re.split(r'<tr>', section_html)
    current_image_type = "rom_image"  # default
    current_size_hint = None

    for row in rows:
        # Check if this row has a label that tells us the image type
        label_match = label_pattern.search(row)
        if label_match:
            label_text = label_match.group(1).lower()
            if 'disk' in label_text or 'hdd' in label_text:
                current_image_type = "disk_image"
            else:
                current_image_type = "rom_image"
            # Extract size hint (192K, 256K, 512K, 1024K)
            size_match = re.search(r'(\d+)\s*K', label_text, re.IGNORECASE)
            current_size_hint = f"{size_match.group(1)}K" if size_match else None

        # Extract all links in this row
        for match in link_pattern.finditer(row):
            url = match.group(1)
            tooltip = match.group(2) or None
            label = re.sub(r'<[^>]+>', '', match.group(3)).strip()
            filename = unquote(urlparse(url).path.split('/')[-1])

            files.append({
                "filename": filename,
                "url": url,
                "label": label if label else None,
                "tooltip": tooltip,
                "image_type": current_image_type,
                "size_hint": current_size_hint,
            })

    return files


def parse_tos_page(html):
    """Parse the TOS page HTML and extract sections with full metadata."""
    sections = []

    # Split by <h3> tags
    parts = re.split(r'<h3>', html)

    for part in parts[1:]:  # Skip content before first <h3>
        # Get section title
        title_match = re.match(r'(.*?)</h3>', part, re.DOTALL)
        if not title_match:
            continue

        title = re.sub(r'<[^>]+>', '', title_match.group(1)).strip()

        # Skip the buggy version notice (it's a sub-header, not a real section)
        if "BUGGY VERSION" in title:
            continue

        # Extract metadata fields
        machines_raw = extract_field(part, "Machines:") or ""
        formats = extract_field(part, "Formats:")
        rom_date = extract_field(part, "ROM Date:")
        gem_report = extract_field(part, "GEM report:")
        teradesk_report = extract_field(part, "TeraDesk 4.08 report:")
        notes = extract_field(part, "Notes:")

        machines = normalize_machine(machines_raw)

        # Extract files with full metadata
        file_entries = extract_files_with_metadata(part)

        # Also get flat link list for download plan (backward compat)
        links = [f["url"] for f in file_entries]

        if not links:
            continue

        # Extract TOS version number from title
        version_match = re.search(r'(?:TOS|EmuTOS|MultiTOS)\s*([\d.]+)', title)
        version = version_match.group(1) if version_match else None

        # Extract codename from title (e.g., "Rainbow TOS", "MEGA TOS")
        codename = None
        codename_match = re.search(r'-\s*(.+)$', title)
        if codename_match:
            codename = codename_match.group(1).strip()

        # Determine category
        if title.startswith("EmuTOS"):
            category = "emutos"
            subfolder = title.replace(" ", "_").replace(".", "_")
        elif title == "Broken TOS Images":
            category = "broken"
            subfolder = None
        elif "MultiTOS" in title:
            category = "official"
            if not machines:
                machines = {"Falcon_030"}
            subfolder = "MultiTOS"
        elif "pre-release" in title.lower() or "pre-release" in machines_raw.lower():
            if not machines:
                category = "pre-release"
                subfolder = title.replace(" ", "_").replace(".", "_")
            else:
                category = "official"
                subfolder = title.split(" - ")[0].strip().replace(" ", "_").replace(".", "_")
        else:
            category = "official"
            subfolder = title.split(" - ")[0].strip().replace(" ", "_").replace(".", "_")

        # Extract description (text right after </h3> before next table row)
        desc_match = re.search(r'</h3>\s*\n?(.*?)(?:</td>|<tr>)', part, re.DOTALL)
        description = None
        if desc_match:
            desc_text = re.sub(r'<[^>]+>', '', desc_match.group(1)).strip()
            desc_text = re.sub(r'\s+', ' ', desc_text).strip()
            if desc_text and len(desc_text) > 10:
                description = desc_text

        sections.append({
            "title": title,
            "version": version,
            "codename": codename,
            "description": description,
            "category": category,
            "machines_raw": machines_raw,
            "machines": machines,
            "formats": formats,
            "rom_date": rom_date,
            "gem_report": gem_report,
            "teradesk_report": teradesk_report,
            "notes": notes,
            "files": file_entries,
            "links": links,
            "subfolder": subfolder,
        })

    return sections


def build_catalog(sections):
    """Build the tos_catalog.json structure from parsed sections."""
    catalog = {
        "metadata": {
            "source_url": PAGE_URL,
            "scraped_at": datetime.now(timezone.utc).isoformat(),
            "total_versions": len(sections),
            "total_files": sum(len(s["files"]) for s in sections),
        },
        "machines": sorted({m for s in sections for m in s["machines"]}),
        "versions": [],
    }

    for section in sections:
        version_entry = {
            "title": section["title"],
            "version": section["version"],
            "codename": section["codename"],
            "description": section["description"],
            "category": section["category"],
            "machines": sorted(section["machines"]),
            "formats": section["formats"],
            "rom_date": section["rom_date"],
            "gem_report": section["gem_report"],
            "teradesk_report": section["teradesk_report"],
            "notes": section["notes"],
            "file_count": len(section["files"]),
            "files": [],
        }

        for f in section["files"]:
            # Build local paths relative to OUTPUT_DIR
            local_paths = []
            if section["category"] == "official" and section["machines"]:
                for machine in sorted(section["machines"]):
                    sub = section["subfolder"] or ""
                    local_paths.append(f"{machine}/{sub}/{f['filename']}")
            elif section["category"] == "emutos":
                sub = section["subfolder"] or ""
                local_paths.append(f"EmuTOS/{sub}/{f['filename']}")
            elif section["category"] == "broken":
                local_paths.append(f"Broken_TOS/{f['filename']}")
            elif section["category"] == "pre-release":
                sub = section["subfolder"] or ""
                local_paths.append(f"Pre-release/{sub}/{f['filename']}")
            else:
                sub = section["subfolder"] or "Unknown"
                local_paths.append(f"Other/{sub}/{f['filename']}")

            # Clean up double slashes
            local_paths = [p.replace("//", "/") for p in local_paths]

            file_entry = {
                "filename": f["filename"],
                "url": f["url"],
                "label": f["label"],
                "tooltip": f["tooltip"],
                "image_type": f["image_type"],
                "size_hint": f["size_hint"],
                "local_paths": local_paths,
            }
            version_entry["files"].append(file_entry)

        catalog["versions"].append(version_entry)

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
            time.sleep(2 ** attempt)  # Exponential backoff

    return False


def get_cache_path(url):
    """Get the cache file path for a URL."""
    # Use URL hash + original filename for cache
    url_hash = hashlib.md5(url.encode()).hexdigest()[:12]
    filename = unquote(urlparse(url).path.split('/')[-1])
    return CACHE_DIR / f"{url_hash}_{filename}"


def main():
    parser = argparse.ArgumentParser(description="Download and organize Atari TOS images")
    parser.add_argument("--dry-run", action="store_true",
                        help="Parse and show what would be downloaded without downloading")
    args = parser.parse_args()

    print(f"Fetching TOS page from {PAGE_URL}...")
    session = requests.Session()
    session.headers.update({
        "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) TOS-Scraper/1.0"
    })

    resp = session.get(PAGE_URL, timeout=30)
    resp.raise_for_status()
    html = resp.text

    print("Parsing TOS sections...")
    sections = parse_tos_page(html)

    # Generate the catalog JSON
    catalog = build_catalog(sections)
    with open(CATALOG_PATH, 'w', encoding='utf-8') as f:
        json.dump(catalog, f, indent=2, ensure_ascii=False)
    print(f"Wrote catalog to {CATALOG_PATH} "
          f"({catalog['metadata']['total_versions']} versions, "
          f"{catalog['metadata']['total_files']} files)")

    # Build download plan: map each URL to its target locations
    # Structure: url -> [(model_folder, tos_subfolder, filename), ...]
    download_plan = {}
    stats = {"total_links": 0, "models": set()}

    for section in sections:
        category = section["category"]
        subfolder = section["subfolder"]
        machines = section["machines"]

        for url in section["links"]:
            filename = unquote(urlparse(url).path.split('/')[-1])
            stats["total_links"] += 1
            targets = []

            if category == "official" and machines:
                for machine in machines:
                    tos_sub = subfolder if subfolder else ""
                    target_dir = OUTPUT_DIR / machine / tos_sub
                    targets.append(target_dir / filename)
                    stats["models"].add(machine)
            elif category == "emutos":
                target_dir = OUTPUT_DIR / "EmuTOS" / subfolder
                targets.append(target_dir / filename)
                stats["models"].add("EmuTOS")
            elif category == "broken":
                target_dir = OUTPUT_DIR / "Broken_TOS"
                targets.append(target_dir / filename)
                stats["models"].add("Broken_TOS")
            elif category == "pre-release":
                target_dir = OUTPUT_DIR / "Pre-release" / subfolder
                targets.append(target_dir / filename)
                stats["models"].add("Pre-release")
            else:
                # Official but no machine identified - put in generic folder
                tos_sub = subfolder if subfolder else "Unknown"
                target_dir = OUTPUT_DIR / "Other" / tos_sub
                targets.append(target_dir / filename)
                stats["models"].add("Other")

            if url not in download_plan:
                download_plan[url] = []
            download_plan[url].extend(targets)

    print(f"\nFound {len(sections)} TOS sections")
    print(f"Total download links: {stats['total_links']}")
    print(f"Unique URLs: {len(download_plan)}")
    print(f"Target model folders: {sorted(stats['models'])}")

    if args.dry_run:
        print("\n=== DRY RUN - showing download plan ===\n")
        for section in sections:
            print(f"  [{section['category']}] {section['title']}")
            if section['machines']:
                print(f"    Machines: {', '.join(sorted(section['machines']))}")
            print(f"    Files: {len(section['links'])}")
        print("\nModel folders that would be created:")
        for model in sorted(stats['models']):
            count = sum(1 for targets in download_plan.values()
                        for t in targets if model in str(t))
            print(f"  {model}/ ({count} files)")
        return

    # Create cache directory
    CACHE_DIR.mkdir(parents=True, exist_ok=True)

    # Download and organize
    downloaded = 0
    skipped = 0
    failed = 0
    total = len(download_plan)

    print(f"\nDownloading {total} unique files...\n")

    for i, (url, targets) in enumerate(download_plan.items(), 1):
        cache_path = get_cache_path(url)
        filename = cache_path.name.split('_', 1)[1] if '_' in cache_path.name else cache_path.name

        # Check if all targets already exist
        all_exist = all(t.exists() for t in targets)
        if all_exist:
            skipped += 1
            if i % 100 == 0:
                print(f"  Progress: {i}/{total} (downloaded: {downloaded}, skipped: {skipped}, failed: {failed})")
            continue

        # Download to cache if not cached
        if not cache_path.exists():
            print(f"  [{i}/{total}] Downloading: {filename}")
            success = download_file(url, cache_path, session)
            if not success:
                failed += 1
                continue
            downloaded += 1
            # Be polite to the server
            time.sleep(0.3)
        else:
            skipped += 1

        # Copy/symlink to all target locations
        for target in targets:
            if target.exists():
                continue
            target.parent.mkdir(parents=True, exist_ok=True)

            # Use the first target as the real file, symlink the rest
            if not any(t.exists() for t in targets):
                # Copy the cached file to the first target
                import shutil
                shutil.copy2(cache_path, target)
            else:
                # Find the first existing target and symlink to it
                existing = next(t for t in targets if t.exists())
                try:
                    target.symlink_to(existing)
                except OSError:
                    # If symlink fails (e.g., cross-device), copy instead
                    import shutil
                    shutil.copy2(cache_path, target)

        if i % 50 == 0:
            print(f"  Progress: {i}/{total} (downloaded: {downloaded}, skipped: {skipped}, failed: {failed})")

    print(f"\n{'='*60}")
    print(f"Download complete!")
    print(f"  Downloaded: {downloaded}")
    print(f"  Skipped (already existed): {skipped}")
    print(f"  Failed: {failed}")
    print(f"\nFiles organized in: {OUTPUT_DIR}")
    print(f"Model folders:")
    for model in sorted(stats['models']):
        model_dir = OUTPUT_DIR / model
        if model_dir.exists():
            file_count = sum(1 for _ in model_dir.rglob("*.zip"))
            print(f"  {model}/ ({file_count} files)")


if __name__ == "__main__":
    main()
