#!/bin/bash
# Build and flash the integration test
# This script:
# 1. Builds the test component EBIN
# 2. Creates a SPIFFS image with the component
# 3. Builds and flashes the integration test app
# 4. Flashes the SPIFFS image
# 5. Monitors output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Port for flashing
PORT="${1:-/dev/ttyACM0}"

echo "=========================================="
echo "  espTari Loader Integration Test"
echo "=========================================="

# Source ESP-IDF
if [ -z "$IDF_PATH" ]; then
    echo "Sourcing ESP-IDF..."
    source ~/esp/esp-idf/export.sh > /dev/null 2>&1
fi

# Step 1: Build test component EBIN
echo ""
echo "Step 1: Building test component..."
cd "$PROJECT_ROOT/tools/ebin_builder"
python3 ebin_builder.py \
    test_component/test_component.c \
    -o test_component/test_component.ebin \
    -t io \
    --debug

# Step 2: Create SPIFFS directory and image
echo ""
echo "Step 2: Creating SPIFFS image..."
mkdir -p "$SCRIPT_DIR/spiffs_data"
cp "$PROJECT_ROOT/tools/ebin_builder/test_component/test_component.ebin" \
   "$SCRIPT_DIR/spiffs_data/"

# List contents
echo "SPIFFS contents:"
ls -la "$SCRIPT_DIR/spiffs_data/"

# Step 3: Build integration test
echo ""
echo "Step 3: Building integration test app..."
cd "$SCRIPT_DIR"
idf.py build

# Step 4: Create SPIFFS image using ESP-IDF tool
echo ""
echo "Step 4: Creating SPIFFS binary..."
python3 $IDF_PATH/components/spiffs/spiffsgen.py \
    0x80000 \
    "$SCRIPT_DIR/spiffs_data" \
    "$SCRIPT_DIR/build/components.bin"

echo "SPIFFS image created: build/components.bin"

# Step 5: Flash everything
echo ""
echo "Step 5: Flashing to $PORT..."
idf.py -p "$PORT" flash

# Flash SPIFFS partition
echo ""
echo "Step 6: Flashing SPIFFS partition..."
python3 -m esptool --chip esp32p4 -p "$PORT" -b 460800 \
    write_flash 0x110000 "$SCRIPT_DIR/build/components.bin"

# Step 7: Monitor
echo ""
echo "Step 7: Starting monitor..."
idf.py -p "$PORT" monitor
