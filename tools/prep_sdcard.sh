#!/bin/bash
#
# prep_sdcard.sh - Prepare SD card for espTari EBIN loading
#
# This script:
#   1. Detects removable SD card devices
#   2. Verifies FAT filesystem
#   3. Creates required directory structure
#   4. Copies EBIN component files
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# EBIN source locations (in order of preference)
EBIN_SOURCES=(
    "$PROJECT_ROOT/tools/ebin_builder/test_component/test_component.ebin"
    "$PROJECT_ROOT/test_apps/esptari_loader_integration/spiffs_data/test_component.ebin"
)

# Target directory on SD card
TARGET_DIR="components"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect removable storage devices
detect_sdcard() {
    log_info "Scanning for removable storage devices..."
    
    local devices=()
    
    # Look for SD card readers (mmcblk devices)
    for dev in /dev/mmcblk[0-9]; do
        if [[ -b "$dev" ]]; then
            # Check if it's removable
            local removable=$(cat /sys/block/$(basename $dev)/removable 2>/dev/null || echo "0")
            if [[ "$removable" == "1" ]]; then
                devices+=("$dev")
            fi
        fi
    done
    
    # Look for USB mass storage (sd* devices, but not sda which is usually system disk)
    for dev in /dev/sd[b-z]; do
        if [[ -b "$dev" ]]; then
            local removable=$(cat /sys/block/$(basename $dev)/removable 2>/dev/null || echo "0")
            if [[ "$removable" == "1" ]]; then
                devices+=("$dev")
            fi
        fi
    done
    
    # Also check for mounted removable media in common locations
    local mounted_media=()
    for mount_point in /media/$USER/* /run/media/$USER/*; do
        if [[ -d "$mount_point" ]]; then
            mounted_media+=("$mount_point")
        fi
    done
    
    if [[ ${#devices[@]} -eq 0 && ${#mounted_media[@]} -eq 0 ]]; then
        log_error "No removable storage devices found!"
        echo ""
        echo "Please insert an SD card and try again."
        echo "If using a USB card reader, make sure it's connected."
        exit 1
    fi
    
    echo ""
    echo "Found devices/mounts:"
    
    local idx=1
    local all_options=()
    
    for dev in "${devices[@]}"; do
        local size=$(lsblk -dn -o SIZE "$dev" 2>/dev/null || echo "unknown")
        local model=$(lsblk -dn -o MODEL "$dev" 2>/dev/null || echo "")
        echo "  $idx) $dev ($size) $model"
        all_options+=("device:$dev")
        ((idx++))
    done
    
    for mount in "${mounted_media[@]}"; do
        local dev=$(df "$mount" 2>/dev/null | tail -1 | awk '{print $1}')
        local size=$(df -h "$mount" 2>/dev/null | tail -1 | awk '{print $2}')
        echo "  $idx) $mount (mounted, $size) - $dev"
        all_options+=("mount:$mount")
        ((idx++))
    done
    
    echo ""
    
    if [[ ${#all_options[@]} -eq 1 ]]; then
        SELECTED_OPTION="${all_options[0]}"
        log_info "Auto-selecting only available option..."
    else
        read -p "Select device/mount [1-$((idx-1))]: " selection
        if [[ ! "$selection" =~ ^[0-9]+$ ]] || [[ "$selection" -lt 1 ]] || [[ "$selection" -gt $((idx-1)) ]]; then
            log_error "Invalid selection"
            exit 1
        fi
        SELECTED_OPTION="${all_options[$((selection-1))]}"
    fi
}

# Verify filesystem and get mount point
verify_and_mount() {
    local option_type="${SELECTED_OPTION%%:*}"
    local option_value="${SELECTED_OPTION#*:}"
    
    if [[ "$option_type" == "mount" ]]; then
        # Already mounted
        MOUNT_POINT="$option_value"
        log_success "Using mounted filesystem at: $MOUNT_POINT"
        
        # Verify it's FAT
        local fstype=$(df -T "$MOUNT_POINT" 2>/dev/null | tail -1 | awk '{print $2}')
        if [[ "$fstype" != "vfat" && "$fstype" != "msdos" && "$fstype" != "fat32" && "$fstype" != "exfat" ]]; then
            log_warn "Filesystem type is '$fstype' (expected vfat/fat32)"
            log_warn "ESP32 SD card library works best with FAT32"
            read -p "Continue anyway? [y/N]: " confirm
            if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
                exit 1
            fi
        else
            log_success "Filesystem: $fstype"
        fi
        return 0
    fi
    
    # It's a device - need to find/create partition and mount
    local device="$option_value"
    local partition=""
    
    # Find first partition
    if [[ "$device" == /dev/mmcblk* ]]; then
        partition="${device}p1"
    else
        partition="${device}1"
    fi
    
    if [[ ! -b "$partition" ]]; then
        log_warn "No partition found on $device"
        log_info "Looking for filesystem directly on device..."
        partition="$device"
    fi
    
    # Check filesystem type
    local fstype=$(lsblk -no FSTYPE "$partition" 2>/dev/null || echo "")
    
    if [[ -z "$fstype" ]]; then
        log_error "No filesystem found on $partition"
        echo ""
        echo "The SD card needs to be formatted with FAT32."
        echo "You can format it with:"
        echo "  sudo mkfs.vfat -F 32 $partition"
        exit 1
    fi
    
    if [[ "$fstype" != "vfat" && "$fstype" != "msdos" && "$fstype" != "exfat" ]]; then
        log_warn "Filesystem type is '$fstype'"
        log_warn "ESP32 SD card library requires FAT filesystem"
        read -p "Continue anyway? [y/N]: " confirm
        if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
            exit 1
        fi
    else
        log_success "Filesystem: $fstype"
    fi
    
    # Check if already mounted
    MOUNT_POINT=$(lsblk -no MOUNTPOINT "$partition" 2>/dev/null || echo "")
    
    if [[ -n "$MOUNT_POINT" ]]; then
        log_success "Already mounted at: $MOUNT_POINT"
        return 0
    fi
    
    # Mount it
    MOUNT_POINT="/tmp/esptari_sdcard_$$"
    log_info "Mounting $partition to $MOUNT_POINT..."
    
    mkdir -p "$MOUNT_POINT"
    
    if ! sudo mount "$partition" "$MOUNT_POINT"; then
        log_error "Failed to mount $partition"
        rmdir "$MOUNT_POINT" 2>/dev/null
        exit 1
    fi
    
    NEEDS_UNMOUNT=1
    log_success "Mounted successfully"
}

# Find EBIN source file
find_ebin_source() {
    for src in "${EBIN_SOURCES[@]}"; do
        if [[ -f "$src" ]]; then
            EBIN_SOURCE="$src"
            log_success "Found EBIN: $src"
            return 0
        fi
    done
    
    log_error "No EBIN file found!"
    echo ""
    echo "Expected locations:"
    for src in "${EBIN_SOURCES[@]}"; do
        echo "  - $src"
    done
    echo ""
    echo "Please build the test component first:"
    echo "  cd $PROJECT_ROOT/tools/ebin_builder/test_component"
    echo "  make"
    exit 1
}

# Create directory structure and copy files
setup_sdcard() {
    local target_path="$MOUNT_POINT/$TARGET_DIR"
    
    log_info "Creating directory structure..."
    
    # Create components directory
    if [[ ! -d "$target_path" ]]; then
        mkdir -p "$target_path"
        log_success "Created: $TARGET_DIR/"
    else
        log_info "Directory exists: $TARGET_DIR/"
    fi
    
    # Copy EBIN file
    local ebin_name=$(basename "$EBIN_SOURCE")
    local dest_file="$target_path/$ebin_name"
    
    log_info "Copying $ebin_name..."
    cp "$EBIN_SOURCE" "$dest_file"
    
    # Verify copy
    local src_size=$(stat -c%s "$EBIN_SOURCE")
    local dst_size=$(stat -c%s "$dest_file")
    
    if [[ "$src_size" -eq "$dst_size" ]]; then
        log_success "Copied: $ebin_name ($src_size bytes)"
    else
        log_error "Size mismatch after copy!"
        exit 1
    fi
    
    # Sync to ensure write completes
    sync
    log_success "Filesystem synced"
}

# Show summary
show_summary() {
    echo ""
    echo "========================================"
    echo -e "${GREEN}SD Card Preparation Complete${NC}"
    echo "========================================"
    echo ""
    echo "SD Card Contents:"
    ls -la "$MOUNT_POINT/$TARGET_DIR/"
    echo ""
    echo "The SD card is ready for use with espTari."
    echo ""
    
    if [[ -n "$NEEDS_UNMOUNT" ]]; then
        echo "To safely remove the SD card:"
        echo "  sudo umount $MOUNT_POINT"
        echo ""
    fi
    
    echo "Expected path on ESP32: /sdcard/$TARGET_DIR/test_component.ebin"
}

# Cleanup on exit
cleanup() {
    if [[ -n "$NEEDS_UNMOUNT" && -n "$MOUNT_POINT" ]]; then
        log_info "Unmounting $MOUNT_POINT..."
        sudo umount "$MOUNT_POINT" 2>/dev/null || true
        rmdir "$MOUNT_POINT" 2>/dev/null || true
    fi
}

trap cleanup EXIT

# Main
main() {
    echo ""
    echo "========================================"
    echo "  espTari SD Card Preparation Script"
    echo "========================================"
    echo ""
    
    detect_sdcard
    verify_and_mount
    find_ebin_source
    setup_sdcard
    show_summary
}

main "$@"
