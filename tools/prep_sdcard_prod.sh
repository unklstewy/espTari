#!/bin/bash
#
# prep_sdcard_prod.sh — Prepare a production SD card for espTari
#
# This script handles the full SD card layout:
#   1. Detect & mount removable SD card
#   2. Create complete directory structure
#   3. Copy machine profiles from sdcard/machines/
#   4. Build Vue+Vite frontend and deploy to www/
#   5. Copy any available EBIN component files
#   6. Copy TOS ROM images (if available)
#   7. Create default config files
#
# Usage:
#   ./tools/prep_sdcard_prod.sh              # interactive device selection
#   ./tools/prep_sdcard_prod.sh /mnt/sdcard  # use specific mount point
#   ./tools/prep_sdcard_prod.sh --no-web     # skip frontend build
#   ./tools/prep_sdcard_prod.sh --dry-run    # show what would be done
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
FRONTEND_DIR="$PROJECT_ROOT/frontend"
SDCARD_TEMPLATE="$PROJECT_ROOT/sdcard"

# Options
OPT_NO_WEB=0
OPT_DRY_RUN=0
OPT_MOUNT_POINT=""
NEEDS_UNMOUNT=0
MOUNT_POINT=""

# Stats
STAT_DIRS=0
STAT_FILES=0
STAT_SKIPPED=0

# ── Colors ────────────────────────────────────────────────────────────

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

log_info()    { echo -e "${BLUE}[INFO]${NC}  $1"; }
log_ok()      { echo -e "${GREEN}[ OK ]${NC}  $1"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC}  $1"; }
log_error()   { echo -e "${RED}[ERR ]${NC}  $1"; }
log_step()    { echo -e "\n${CYAN}${BOLD}── $1 ──${NC}"; }
log_dry()     { echo -e "${YELLOW}[DRY]${NC}  $1"; }

# ── Parse arguments ───────────────────────────────────────────────────

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --no-web)    OPT_NO_WEB=1; shift ;;
            --dry-run)   OPT_DRY_RUN=1; shift ;;
            --help|-h)   show_help; exit 0 ;;
            -*)          log_error "Unknown option: $1"; show_help; exit 1 ;;
            *)           OPT_MOUNT_POINT="$1"; shift ;;
        esac
    done
}

show_help() {
    cat <<EOF

Usage: $(basename "$0") [OPTIONS] [MOUNT_POINT]

Prepare a production SD card for espTari.

Options:
  --no-web      Skip building/deploying the Vue frontend
  --dry-run     Show what would be done without writing anything
  -h, --help    Show this help

Arguments:
  MOUNT_POINT   Path to mounted SD card (auto-detected if omitted)

Directory layout created on SD card:
  /config/              Configuration files
    /cores/{cpu,mmu,video,audio,io,system,misc}/   EBIN component binaries
  /disks/{floppy,hard}/                Disk images
  /machines/            Machine profile JSON files
  /roms/{tos,cartridges,bios}/         ROM images
  /screenshots/         Saved screenshots
  /states/              Save states
  /www/                 Web interface (Vue SPA)

EOF
}

# ── SD card detection (reused from prep_sdcard.sh) ────────────────────

detect_sdcard() {
    log_step "Detecting SD Card"

    local devices=()
    local mounted_media=()

    # Look for SD card readers (mmcblk devices)
    for dev in /dev/mmcblk[0-9]; do
        if [[ -b "$dev" ]]; then
            local removable
            removable=$(cat "/sys/block/$(basename "$dev")/removable" 2>/dev/null || echo "0")
            [[ "$removable" == "1" ]] && devices+=("$dev")
        fi
    done

    # USB mass storage (skip sda = system disk, skip nvme)
    for dev in /dev/sd[b-z]; do
        if [[ -b "$dev" ]]; then
            local removable
            removable=$(cat "/sys/block/$(basename "$dev")/removable" 2>/dev/null || echo "0")
            [[ "$removable" == "1" ]] && devices+=("$dev")
        fi
    done

    # Already-mounted removable media (skip nvme-backed mounts)
    for mount_point in /media/"$USER"/* /run/media/"$USER"/*; do
        if [[ -d "$mount_point" ]]; then
            local backing_dev
            backing_dev=$(df "$mount_point" 2>/dev/null | tail -1 | awk '{print $1}')
            if [[ "$backing_dev" == *nvme* ]]; then
                continue  # never touch NVMe devices
            fi
            mounted_media+=("$mount_point")
        fi
    done

    if [[ ${#devices[@]} -eq 0 && ${#mounted_media[@]} -eq 0 ]]; then
        log_error "No removable storage devices found!"
        echo ""
        echo "  Insert an SD card and try again, or specify a mount point:"
        echo "    $0 /path/to/sdcard"
        exit 1
    fi

    local idx=1
    local all_options=()

    echo ""
    echo "  Found devices/mounts:"
    for dev in "${devices[@]}"; do
        local size model
        size=$(lsblk -dn -o SIZE "$dev" 2>/dev/null || echo "?")
        model=$(lsblk -dn -o MODEL "$dev" 2>/dev/null || echo "")
        echo "    $idx) $dev ($size) $model"
        all_options+=("device:$dev")
        ((idx++))
    done
    for mount in "${mounted_media[@]}"; do
        local mdev msize
        mdev=$(df "$mount" 2>/dev/null | tail -1 | awk '{print $1}')
        msize=$(df -h "$mount" 2>/dev/null | tail -1 | awk '{print $2}')
        echo "    $idx) $mount ($msize) — $mdev"
        all_options+=("mount:$mount")
        ((idx++))
    done
    echo ""

    if [[ ${#all_options[@]} -eq 1 ]]; then
        SELECTED_OPTION="${all_options[0]}"
        log_info "Only one device found — auto-selected"
    else
        read -rp "  Select device/mount [1-$((idx-1))]: " selection
        if [[ ! "$selection" =~ ^[0-9]+$ ]] || (( selection < 1 || selection > idx-1 )); then
            log_error "Invalid selection"; exit 1
        fi
        SELECTED_OPTION="${all_options[$((selection-1))]}"
    fi

    # Always confirm before writing — don't silently nuke someone's card
    local sel_type="${SELECTED_OPTION%%:*}"
    local sel_value="${SELECTED_OPTION#*:}"
    echo ""
    echo -e "  ${YELLOW}${BOLD}⚠  ALL existing files in www/ on this card will be replaced.${NC}"
    echo -e "  ${BOLD}Target: ${sel_value}${NC}"
    echo ""
    read -rp "  Proceed? [y/N]: " confirm
    if [[ "$confirm" != [yY] ]]; then
        log_info "Aborted by user"
        exit 0
    fi
}

verify_and_mount() {
    local option_type="${SELECTED_OPTION%%:*}"
    local option_value="${SELECTED_OPTION#*:}"

    if [[ "$option_type" == "mount" ]]; then
        MOUNT_POINT="$option_value"
        log_ok "Using mounted filesystem: $MOUNT_POINT"
        return 0
    fi

    local device="$option_value"
    local partition

    if [[ "$device" == /dev/mmcblk* ]]; then
        partition="${device}p1"
    else
        partition="${device}1"
    fi
    [[ ! -b "$partition" ]] && partition="$device"

    local fstype
    fstype=$(lsblk -no FSTYPE "$partition" 2>/dev/null || echo "")

    if [[ -z "$fstype" ]]; then
        log_error "No filesystem on $partition — format with: sudo mkfs.vfat -F 32 $partition"
        exit 1
    fi

    if [[ "$fstype" != "vfat" && "$fstype" != "msdos" && "$fstype" != "exfat" ]]; then
        log_warn "Filesystem '$fstype' — ESP32 works best with FAT32"
        read -rp "  Continue anyway? [y/N]: " confirm
        [[ "$confirm" != [yY] ]] && exit 1
    fi

    # Check if already mounted
    MOUNT_POINT=$(lsblk -no MOUNTPOINT "$partition" 2>/dev/null || echo "")

    if [[ -n "$MOUNT_POINT" ]]; then
        log_ok "Already mounted at: $MOUNT_POINT"
        return 0
    fi

    MOUNT_POINT="/tmp/esptari_sdcard_$$"
    log_info "Mounting $partition → $MOUNT_POINT"
    mkdir -p "$MOUNT_POINT"
    if ! sudo mount "$partition" "$MOUNT_POINT"; then
        log_error "Failed to mount $partition"
        rmdir "$MOUNT_POINT" 2>/dev/null
        exit 1
    fi
    NEEDS_UNMOUNT=1
    log_ok "Mounted"
}

# ── Helper: create dir on card ────────────────────────────────────────

ensure_dir() {
    local dir="$MOUNT_POINT/$1"
    if [[ ! -d "$dir" ]]; then
        if [[ $OPT_DRY_RUN -eq 1 ]]; then
            log_dry "mkdir $1/"
        else
            mkdir -p "$dir"
        fi
        STAT_DIRS=$((STAT_DIRS + 1))
    fi
}

# ── Helper: copy file to card ─────────────────────────────────────────

copy_file() {
    local src="$1"
    local dest_dir="$2"
    local dest_name="${3:-$(basename "$src")}"
    local dest="$MOUNT_POINT/$dest_dir/$dest_name"

    ensure_dir "$dest_dir"

    if [[ $OPT_DRY_RUN -eq 1 ]]; then
        log_dry "cp $(basename "$src") → $dest_dir/$dest_name"
        STAT_FILES=$((STAT_FILES + 1))
        return 0
    fi

    cp "$src" "$dest"
    local src_size dst_size
    src_size=$(stat -c%s "$src")
    dst_size=$(stat -c%s "$dest")

    if [[ "$src_size" -eq "$dst_size" ]]; then
        log_ok "$dest_dir/$dest_name ($src_size bytes)"
        STAT_FILES=$((STAT_FILES + 1))
    else
        log_error "Size mismatch copying $dest_name!"
        exit 1
    fi
}

# ── Step 1: Directory structure ───────────────────────────────────────

create_directories() {
    log_step "Creating Directory Structure"

    local dirs=(
        "config"
        "cores/cpu"
        "cores/mmu"
        "cores/video"
        "cores/audio"
        "cores/io"
        "cores/system"
        "cores/misc"
        "disks/floppy"
        "disks/hard"
        "machines"
        "roms/tos"
        "roms/cartridges"
        "roms/bios"
        "screenshots"
        "states"
        "www"
    )

    for d in "${dirs[@]}"; do
        ensure_dir "$d"
    done

    log_ok "Directory structure ready (${STAT_DIRS} created)"
}

# ── Step 2: Machine profiles ─────────────────────────────────────────

copy_machine_profiles() {
    log_step "Machine Profiles"

    local count=0
    for json in "$SDCARD_TEMPLATE/machines/"*.json; do
        [[ -f "$json" ]] || continue
        copy_file "$json" "machines"
        count=$((count + 1))
    done

    if [[ $count -eq 0 ]]; then
        log_warn "No machine profiles found in sdcard/machines/"
    else
        log_ok "$count machine profile(s)"
    fi
}

# ── Step 3: Build & deploy frontend ──────────────────────────────────

build_and_deploy_frontend() {
    if [[ $OPT_NO_WEB -eq 1 ]]; then
        log_step "Web Frontend (skipped — --no-web)"
        return 0
    fi

    log_step "Web Frontend"

    if [[ ! -f "$FRONTEND_DIR/package.json" ]]; then
        log_warn "No frontend/package.json — skipping web build"
        return 0
    fi

    # Check for node/npm
    if ! command -v npm &>/dev/null; then
        log_warn "npm not found — skipping web build"
        log_warn "Install Node.js and re-run, or use --no-web"
        return 0
    fi

    # Install deps if needed
    if [[ ! -d "$FRONTEND_DIR/node_modules" ]]; then
        log_info "Installing frontend dependencies..."
        (cd "$FRONTEND_DIR" && npm install --silent)
        log_ok "Dependencies installed"
    fi

    # Build
    log_info "Building Vue+Vite frontend..."
    (cd "$FRONTEND_DIR" && npm run build --silent 2>&1)
    
    if [[ ! -d "$FRONTEND_DIR/dist" || ! -f "$FRONTEND_DIR/dist/index.html" ]]; then
        log_error "Frontend build failed — dist/index.html not found"
        return 1
    fi

    local file_count
    file_count=$(find "$FRONTEND_DIR/dist" -type f | wc -l)
    log_ok "Built $file_count files"

    # Deploy to www/
    log_info "Deploying to www/..."
    ensure_dir "www"

    if [[ $OPT_DRY_RUN -eq 1 ]]; then
        log_dry "rsync frontend/dist/ → www/"
        STAT_FILES=$((STAT_FILES + file_count))
    else
        # Clean existing www/ content first
        rm -rf "${MOUNT_POINT:?}/www/"*

        # Copy all built files preserving structure
        cp -r "$FRONTEND_DIR/dist/." "$MOUNT_POINT/www/"

        local deployed
        deployed=$(find "$MOUNT_POINT/www" -type f | wc -l)
        log_ok "Deployed $deployed files to www/"
        STAT_FILES=$((STAT_FILES + deployed))
    fi
}

# ── Step 4: EBIN components ──────────────────────────────────────────

copy_ebin_components() {
    log_step "EBIN Components"

    # Define search paths for each component:
    #   name  dest_dir  search_path1  search_path2  ...
    local -a components=(
        "m68000.ebin|cores/cpu|$PROJECT_ROOT/build/ebins/cpu/m68000.ebin|$PROJECT_ROOT/test_apps/m68000_cpu_test/m68000.ebin|/tmp/m68000.ebin"
        "mfp68901.ebin|cores/io|$PROJECT_ROOT/build/ebins/misc/mfp68901.ebin|/tmp/mfp68901.ebin"
        "shifter.ebin|cores/video|$PROJECT_ROOT/build/ebins/video/shifter.ebin|/tmp/shifter.ebin"
        "ym2149.ebin|cores/audio|$PROJECT_ROOT/build/ebins/audio/ym2149.ebin|/tmp/ym2149.ebin"
        "blitter.ebin|cores/io|$PROJECT_ROOT/build/ebins/misc/blitter.ebin|/tmp/blitter.ebin"
        "fdc_wd1772.ebin|cores/io|$PROJECT_ROOT/build/ebins/misc/fdc_wd1772.ebin|/tmp/fdc_wd1772.ebin"
        "st_mmu.ebin|cores/mmu|$PROJECT_ROOT/build/ebins/mmu/st_mmu.ebin|/tmp/st_mmu.ebin"
        "ste_mmu.ebin|cores/mmu|$PROJECT_ROOT/build/ebins/mmu/ste_mmu.ebin|/tmp/ste_mmu.ebin"
        "ste_shifter.ebin|cores/video|$PROJECT_ROOT/build/ebins/video/ste_shifter.ebin|/tmp/ste_shifter.ebin"
        "dma_audio.ebin|cores/audio|$PROJECT_ROOT/build/ebins/audio/dma_audio.ebin|/tmp/dma_audio.ebin"
        "st_monolith.ebin|cores/system|$PROJECT_ROOT/build/ebins/system/st_monolith.ebin|/tmp/st_monolith.ebin"
        "test_component.ebin|cores/io|$PROJECT_ROOT/tools/ebin_builder/test_component/test_component.ebin|$PROJECT_ROOT/test_apps/esptari_loader_integration/spiffs_data/test_component.ebin"
    )

    local found=0
    for entry in "${components[@]}"; do
        IFS='|' read -ra parts <<< "$entry"
        local name="${parts[0]}"
        local dest="${parts[1]}"
        local located=""

        for (( i=2; i<${#parts[@]}; i++ )); do
            if [[ -f "${parts[$i]}" ]]; then
                located="${parts[$i]}"
                break
            fi
        done

        if [[ -n "$located" ]]; then
            copy_file "$located" "$dest" "$name"
            found=$((found + 1))
        else
            STAT_SKIPPED=$((STAT_SKIPPED + 1))
        fi
    done

    if [[ $found -eq 0 ]]; then
        log_warn "No EBIN files found (build components first)"
    else
        log_ok "$found EBIN(s) copied"
    fi
    [[ $STAT_SKIPPED -gt 0 ]] && log_info "$STAT_SKIPPED component(s) not found (skipped)"
}

# ── Step 5: TOS ROMs ─────────────────────────────────────────────────

copy_tos_roms() {
    log_step "TOS ROM Images"

    # Search common locations for TOS images
    local -a search_dirs=(
        "$PROJECT_ROOT/assets/emulator"
        "$HOME"
        "/tmp"
    )

    local found=0

    # Look for any .img files matching TOS naming patterns
    local -a tos_patterns=(
        "tos*.img"
        "tos*.bin"
        "tos*.rom"
        "TOS*.img"
        "TOS*.bin"
        "TOS*.rom"
        "emutos*.img"
        "emutos*.bin"
    )

    local -A seen_files  # dedup by filename

    for dir in "${search_dirs[@]}"; do
        [[ -d "$dir" ]] || continue
        for pattern in "${tos_patterns[@]}"; do
            while IFS= read -r -d '' romfile; do
                local name
                name=$(basename "$romfile")
                if [[ -z "${seen_files[$name]+x}" ]]; then
                    seen_files[$name]=1
                    copy_file "$romfile" "roms/tos" "$name"
                    found=$((found + 1))
                fi
            done < <(find "$dir" -maxdepth 3 -name "$pattern" -type f -print0 2>/dev/null)
        done
    done

    # Also copy from the assets/emulator TOS catalog directories
    for dir in "$PROJECT_ROOT/assets/emulator/"*/; do
        [[ -d "$dir" ]] || continue
        for romfile in "$dir"*.img "$dir"*.bin "$dir"*.rom; do
            [[ -f "$romfile" ]] || continue
            local name
            name=$(basename "$romfile")
            if [[ -z "${seen_files[$name]+x}" ]]; then
                seen_files[$name]=1
                copy_file "$romfile" "roms/tos" "$name"
                found=$((found + 1))
            fi
        done
    done

    if [[ $found -eq 0 ]]; then
        log_warn "No TOS ROM images found"
        log_info "Place .img/.bin/.rom files in ~/  or  assets/emulator/"
        log_info "EmuTOS (free) available at: https://emutos.sourceforge.io"
    else
        log_ok "$found ROM image(s) copied"
    fi
}

# ── Step 6: Default config ───────────────────────────────────────────

create_default_config() {
    log_step "Configuration"

    local config_file="$MOUNT_POINT/config/esptari.json"

    if [[ -f "$config_file" ]]; then
        log_ok "Existing config preserved: config/esptari.json"
        return 0
    fi

    if [[ $OPT_DRY_RUN -eq 1 ]]; then
        log_dry "Create config/esptari.json"
        return 0
    fi

    ensure_dir "config"

    cat > "$config_file" <<'CONFIGEOF'
{
    "machine": "st",
    "tos_rom": "auto",
    "memory_kb": 1024,
    "auto_start": false,
    "web_port": 80,
    "stream_port": 8080,
    "display": {
        "resolution": "low",
        "palette": "st"
    },
    "audio": {
        "enabled": true,
        "sample_rate": 44100
    },
    "input": {
        "mouse_speed": 1.0,
        "joystick_port": 1
    }
}
CONFIGEOF

    log_ok "Created default config/esptari.json"
    STAT_FILES=$((STAT_FILES + 1))
}

# ── Summary ───────────────────────────────────────────────────────────

show_summary() {
    echo ""
    echo -e "${BOLD}════════════════════════════════════════${NC}"
    echo -e "${GREEN}${BOLD}  espTari SD Card — Ready${NC}"
    echo -e "${BOLD}════════════════════════════════════════${NC}"
    echo ""
    
    if [[ $OPT_DRY_RUN -eq 1 ]]; then
        echo -e "  ${YELLOW}(DRY RUN — nothing was written)${NC}"
        echo ""
    fi
    
    echo "  Directories: $STAT_DIRS created"
    echo "  Files:       $STAT_FILES copied"
    [[ $STAT_SKIPPED -gt 0 ]] && echo "  Skipped:     $STAT_SKIPPED (not found)"
    echo ""
    
    # Show tree if 'tree' is available, otherwise ls
    if command -v tree &>/dev/null && [[ $OPT_DRY_RUN -eq 0 ]]; then
        echo "  Card contents:"
        tree -L 2 --dirsfirst "$MOUNT_POINT" 2>/dev/null | sed 's/^/    /'
    else
        echo "  Card layout:"
        for d in config cores disks machines roms screenshots states www; do
            local full="$MOUNT_POINT/$d"
            if [[ -d "$full" ]]; then
                local cnt
                cnt=$(find "$full" -type f 2>/dev/null | wc -l)
                printf "    %-20s %d file(s)\n" "$d/" "$cnt"
            fi
        done
    fi

    echo ""
    
    if [[ $NEEDS_UNMOUNT -eq 1 ]]; then
        echo "  Safely remove:"
        echo "    sudo umount $MOUNT_POINT"
        echo ""
    fi

    echo "  Mount point on ESP32: /sdcard"
    echo "  Web UI served from:   /sdcard/www/"
    echo ""
}

# ── Cleanup ───────────────────────────────────────────────────────────

cleanup() {
    if [[ $NEEDS_UNMOUNT -eq 1 && -n "$MOUNT_POINT" ]]; then
        log_info "Unmounting $MOUNT_POINT..."
        sudo umount "$MOUNT_POINT" 2>/dev/null || true
        rmdir "$MOUNT_POINT" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# ── Main ──────────────────────────────────────────────────────────────

main() {
    echo ""
    echo -e "${BOLD}════════════════════════════════════════${NC}"
    echo -e "${BOLD}  espTari Production SD Card Setup${NC}"
    echo -e "${BOLD}════════════════════════════════════════${NC}"
    echo ""

    parse_args "$@"

    # Resolve mount point
    if [[ -n "$OPT_MOUNT_POINT" ]]; then
        if [[ ! -d "$OPT_MOUNT_POINT" ]]; then
            log_error "Mount point does not exist: $OPT_MOUNT_POINT"
            exit 1
        fi
        MOUNT_POINT="$OPT_MOUNT_POINT"
        log_ok "Using specified mount point: $MOUNT_POINT"
    else
        detect_sdcard
        verify_and_mount
    fi

    # Sanity check
    if [[ -z "$MOUNT_POINT" || ! -d "$MOUNT_POINT" ]]; then
        log_error "No valid mount point"; exit 1
    fi

    local free_mb
    free_mb=$(df -BM "$MOUNT_POINT" 2>/dev/null | tail -1 | awk '{gsub(/M/,""); print $4}')
    log_info "Free space: ${free_mb}MB"

    if [[ -n "$free_mb" ]] && (( free_mb < 10 )); then
        log_error "Less than 10MB free — please use a larger card"
        exit 1
    fi

    create_directories
    copy_machine_profiles
    build_and_deploy_frontend
    copy_ebin_components
    copy_tos_roms
    create_default_config

    # Final sync
    if [[ $OPT_DRY_RUN -eq 0 ]]; then
        sync
        log_ok "Filesystem synced"
    fi

    show_summary

    # Auto-unmount after successful prep
    if [[ $OPT_DRY_RUN -eq 0 ]]; then
        log_info "Unmounting SD card..."
        if [[ $NEEDS_UNMOUNT -eq 1 ]]; then
            sudo umount "$MOUNT_POINT" 2>/dev/null && {
                rmdir "$MOUNT_POINT" 2>/dev/null || true
                NEEDS_UNMOUNT=0  # prevent cleanup trap from trying again
                log_ok "SD card unmounted — safe to remove"
            } || log_warn "Failed to unmount — eject manually"
        else
            # Card was already mounted by the OS (e.g. /media/user/...)
            umount "$MOUNT_POINT" 2>/dev/null || sudo umount "$MOUNT_POINT" 2>/dev/null && {
                log_ok "SD card unmounted — safe to remove"
            } || log_warn "Could not unmount $MOUNT_POINT — eject manually"
        fi
    fi
}

main "$@"
