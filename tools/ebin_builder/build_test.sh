#!/bin/bash
# Build the test component EBIN
# Run from the tools/ebin_builder directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Source ESP-IDF to get toolchain in PATH
if [ -z "$IDF_PATH" ]; then
    echo "Sourcing ESP-IDF..."
    source ~/esp/esp-idf/export.sh > /dev/null 2>&1
fi

echo "Building test_component.ebin..."
python3 ebin_builder.py \
    test_component/test_component.c \
    -o test_component/test_component.ebin \
    -t io \
    --debug \
    -v

echo ""
echo "Verifying EBIN..."
python3 -c "
import struct
with open('test_component/test_component.ebin', 'rb') as f:
    data = f.read(60)
    header = struct.unpack('<IHHIIIIIIIIIIIII', data)
    print(f'Magic:      0x{header[0]:08X} ({\"VALID\" if header[0] == 0x4E494245 else \"INVALID\"})')
    print(f'Version:    {header[1]}')
    print(f'Type:       {header[2]} (io)')
    print(f'Flags:      0x{header[3]:08X}')
    print(f'Code size:  {header[4]} bytes')
    print(f'Data size:  {header[5]} bytes')
    print(f'BSS size:   {header[6]} bytes')
    print(f'Entry off:  {header[7]}')
    print(f'Interface:  0x{header[8]:08X}')
    print(f'Min RAM:    {header[9]} bytes')
    print(f'Relocs:     {header[10]}')
"

echo ""
echo "Done! EBIN file: test_component/test_component.ebin"
