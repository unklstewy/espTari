#!/usr/bin/env python3
"""
EBIN Builder Tool

Compiles C source code into EBIN format for the espTari dynamic loader.
Uses RISC-V toolchain to create position-independent code and generates
relocations for runtime loading into PSRAM.

SPDX-License-Identifier: MIT
"""

import argparse
import struct
import subprocess
import sys
import os
import tempfile
from pathlib import Path
from dataclasses import dataclass
from typing import List, Dict, Optional, Tuple
import re

# EBIN format constants
EBIN_MAGIC = 0x4E494245  # "EBIN"
EBIN_VERSION = 1

# Component types
COMPONENT_TYPES = {
    'cpu': 1,
    'video': 2,
    'audio': 3,
    'io': 4,
}

# Relocation types
EBIN_RELOC_ABSOLUTE = 0
EBIN_RELOC_RELATIVE = 1
EBIN_RELOC_HIGH16 = 2
EBIN_RELOC_LOW16 = 3

# RISC-V relocation types we handle
R_RISCV_32 = 1
R_RISCV_64 = 2
R_RISCV_BRANCH = 16
R_RISCV_JAL = 17
R_RISCV_CALL = 18
R_RISCV_PCREL_HI20 = 23
R_RISCV_PCREL_LO12_I = 24
R_RISCV_HI20 = 26
R_RISCV_LO12_I = 27
R_RISCV_LO12_S = 28


@dataclass
class Relocation:
    """EBIN relocation entry"""
    offset: int
    reloc_type: int
    section: int  # 0=code, 1=data


@dataclass
class Section:
    """ELF section data"""
    name: str
    data: bytes
    addr: int
    size: int


@dataclass
class EbinConfig:
    """EBIN build configuration"""
    sources: List[str]
    output: str
    component_type: str
    entry_symbol: str
    interface_version: int
    min_ram: int
    flags: int
    include_dirs: List[str]
    defines: List[str]
    toolchain_prefix: str
    debug: bool


def find_toolchain() -> str:
    """Find RISC-V toolchain prefix"""
    # Try common ESP-IDF toolchain locations
    prefixes = [
        'riscv32-esp-elf-',
        os.path.expanduser('~/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20241119/riscv32-esp-elf/bin/riscv32-esp-elf-'),
    ]
    
    for prefix in prefixes:
        gcc = f'{prefix}gcc'
        try:
            result = subprocess.run([gcc, '--version'], capture_output=True, text=True)
            if result.returncode == 0:
                return prefix
        except FileNotFoundError:
            continue
    
    raise RuntimeError("Could not find RISC-V toolchain. Ensure ESP-IDF is installed and sourced.")


def compile_sources(config: EbinConfig, output_dir: str) -> List[str]:
    """Compile C sources to object files (one per source)
    
    Returns list of object file paths.
    """
    gcc = f'{config.toolchain_prefix}gcc'
    
    # Common compile flags
    common_flags = [
        '-c',
        '-fPIC',                    # Position-independent code
        '-fno-common',              # No common symbols
        '-ffunction-sections',      # Each function in its own section
        '-fdata-sections',          # Each data item in its own section
        '-march=rv32imafc',         # ESP32-P4 architecture
        '-mabi=ilp32f',             # ABI
        '-mno-relax',               # Disable linker relaxation (keeps auipc+addi pairs)
        '-O2',                      # Optimize
        '-Wall',
        '-Wextra',
        '-nostdlib',                # No standard library
        '-ffreestanding',           # Freestanding environment
    ]
    
    if config.debug:
        common_flags.append('-g')
    
    # Add include directories
    for inc in config.include_dirs:
        common_flags.extend(['-I', inc])
    
    # Add defines
    for define in config.defines:
        common_flags.extend(['-D', define])
    
    obj_files = []
    for src in config.sources:
        # Derive object file name from source file name
        src_basename = os.path.splitext(os.path.basename(src))[0]
        obj_file = os.path.join(output_dir, f'{src_basename}.o')
        
        cmd = [gcc] + common_flags + ['-o', obj_file, src]
        
        print(f"Compiling: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"Compilation failed for {src}:\n{result.stderr}")
            sys.exit(1)
        
        obj_files.append(obj_file)
    
    return obj_files


def link_object(config: EbinConfig, obj_files: List[str], output_elf: str, linker_script: str) -> None:
    """Link object files with linker script"""
    ld = f'{config.toolchain_prefix}ld'
    
    cmd = [
        ld,
        '-T', linker_script,
        '-o', output_elf,
        '--entry', config.entry_symbol,
        '-nostdlib',
        '--no-relax',  # Disable relaxation (preserves auipc+addi pairs)
        '--gc-sections',
        '-q',  # Emit relocations
    ] + obj_files
    
    print(f"Linking: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"Linking failed:\n{result.stderr}")
        sys.exit(1)


def extract_sections(config: EbinConfig, elf_file: str) -> Tuple[bytes, bytes, int]:
    """Extract code and data sections from ELF"""
    objcopy = f'{config.toolchain_prefix}objcopy'
    readelf = f'{config.toolchain_prefix}readelf'
    
    # Read section headers
    result = subprocess.run([readelf, '-S', elf_file], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"readelf failed:\n{result.stderr}")
        sys.exit(1)
    
    # Parse section info
    sections = {}
    for line in result.stdout.split('\n'):
        # Example: [Nr] Name Type Addr Off Size ES Flg Lk Inf Al
        match = re.search(r'\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)', line)
        if match:
            name = match.group(1)
            addr = int(match.group(2), 16)
            offset = int(match.group(3), 16)
            size = int(match.group(4), 16)
            sections[name] = {'addr': addr, 'offset': offset, 'size': size}
    
    # Extract .text + .rodata as unified code section
    # (.rodata must be contiguous with .text for PC-relative addressing to work)
    code_bin = tempfile.mktemp(suffix='.bin')
    cmd = [objcopy, '-O', 'binary', '-j', '.text', '-j', '.rodata', elf_file, code_bin]
    subprocess.run(cmd, check=True)
    with open(code_bin, 'rb') as f:
        code_data = f.read()
    os.unlink(code_bin)
    
    # Extract .data + .got + .got.plt as unified data section
    # The GOT must be contiguous with .data so PC-relative auipc+lw reaches it.
    # We include all writable non-BSS sections in a single binary extraction.
    data_bin = tempfile.mktemp(suffix='.bin')
    data_sections = ['-j', '.data']
    if '.got' in sections:
        data_sections += ['-j', '.got']
    cmd = [objcopy, '-O', 'binary'] + data_sections + [elf_file, data_bin]
    result = subprocess.run(cmd, capture_output=True)
    if result.returncode == 0 and os.path.exists(data_bin):
        with open(data_bin, 'rb') as f:
            data_data = f.read()
        os.unlink(data_bin)
    else:
        data_data = b''
    
    # Get BSS size
    bss_size = sections.get('.bss', {}).get('size', 0)
    
    return code_data, data_data, bss_size


def get_entry_offset(config: EbinConfig, elf_file: str) -> int:
    """Get entry point offset within code section"""
    readelf = f'{config.toolchain_prefix}readelf'
    nm = f'{config.toolchain_prefix}nm'
    
    # Get symbol addresses
    result = subprocess.run([nm, elf_file], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"nm failed:\n{result.stderr}")
        sys.exit(1)
    
    entry_addr = None
    text_addr = None
    
    for line in result.stdout.split('\n'):
        parts = line.split()
        if len(parts) >= 3:
            addr = int(parts[0], 16)
            symbol = parts[2]
            if symbol == config.entry_symbol:
                entry_addr = addr
    
    # Get .text section address
    result = subprocess.run([readelf, '-S', elf_file], capture_output=True, text=True)
    for line in result.stdout.split('\n'):
        if '.text' in line:
            match = re.search(r'\.text\s+\S+\s+([0-9a-fA-F]+)', line)
            if match:
                text_addr = int(match.group(1), 16)
                break
    
    if entry_addr is None:
        print(f"Entry symbol '{config.entry_symbol}' not found")
        sys.exit(1)
    
    if text_addr is None:
        text_addr = 0
    
    return entry_addr - text_addr


def extract_relocations(config: EbinConfig, elf_file: str) -> List[Relocation]:
    """Extract relocations from ELF file"""
    readelf = f'{config.toolchain_prefix}readelf'
    objcopy = f'{config.toolchain_prefix}objcopy'
    
    # First, get section addresses and sizes
    result = subprocess.run([readelf, '-S', elf_file], capture_output=True, text=True)
    section_addrs = {}
    section_sizes = {}
    for line in result.stdout.split('\n'):
        # Parse section entries: [Nr] Name Type Addr Off Size ...
        # Match sections precisely (exact name match to avoid .rela.text matching .text)
        for sec_name in ['text', 'data', 'rodata', 'got', 'bss']:
            # Use word boundary matching: section name followed by whitespace and type
            pattern = r'\.' + sec_name + r'\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)'
            if re.search(r'\]\s+\.' + sec_name + r'\s', line):
                match = re.search(pattern, line)
                if match:
                    section_addrs[sec_name] = int(match.group(2), 16)
                    section_sizes[sec_name] = int(match.group(4), 16)
    
    text_base = section_addrs.get('text', 0)
    data_base = section_addrs.get('data', 0)
    rodata_base = section_addrs.get('rodata', 0)
    
    result = subprocess.run([readelf, '-r', elf_file], capture_output=True, text=True)
    if result.returncode != 0:
        return []  # No relocations
    
    relocations = []
    current_section = 0  # 0=code, 1=data
    current_base = text_base
    in_data_reloc = False
    
    for line in result.stdout.split('\n'):
        if 'Relocation section' in line:
            if '.rela.data' in line:
                current_section = 1
                current_base = data_base
                in_data_reloc = True
            elif '.rela.rodata' in line:
                current_section = 1
                current_base = rodata_base  
                in_data_reloc = True
            else:
                current_section = 0
                current_base = text_base
                in_data_reloc = False
        
        # Only process absolute relocations in data section
        # PC-relative relocations in .text are handled by auipc at runtime
        if not in_data_reloc and current_section == 0:
            continue
        
        # Parse relocation entry
        # Offset Info Type Sym. Value Sym. Name + Addend
        match = re.search(r'([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+(\S+)', line)
        if match and match.group(2) == 'R_RISCV_32':
            # With -q linking, offsets are VMA addresses - normalize to section-relative
            offset = int(match.group(1), 16) - current_base
            
            relocations.append(Relocation(
                offset=offset,
                reloc_type=EBIN_RELOC_ABSOLUTE,
                section=current_section
            ))
    
    # Synthesize relocations for GOT entries
    # With -fPIC, the compiler accesses globals via GOT. The GOT is merged into
    # .data by our linker script, but the linker doesn't emit relocations for
    # resolved GOT entries. We scan the GOT content and add relocations for any
    # entries that contain valid EBIN-internal addresses.
    got_addr = section_addrs.get('got')
    got_size = section_sizes.get('got', 0)
    
    if got_addr is not None and got_size > 0:
        # Total addressable range of the EBIN
        bss_end = section_addrs.get('bss', 0) + section_sizes.get('bss', 0)
        
        # Read GOT content via readelf hex dump
        got_result = subprocess.run([readelf, '-x', '.got', elf_file],
                                     capture_output=True, text=True)
        
        # Parse the hex dump to read GOT entries
        data_bytes = {}
        for hline in got_result.stdout.split('\n'):
            hmatch = re.match(r'\s+0x([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)', hline)
            if hmatch:
                addr = int(hmatch.group(1), 16)
                for i, grp in enumerate([hmatch.group(2), hmatch.group(3), hmatch.group(4), hmatch.group(5)]):
                    word_bytes = bytes.fromhex(grp)
                    data_bytes[addr + i * 4] = struct.unpack('<I', word_bytes)[0]
        
        # Check each GOT entry
        got_reloc_count = 0
        existing_offsets = {r.offset for r in relocations if r.section == 1}
        
        for i in range(0, got_size, 4):
            entry_addr = got_addr + i
            entry_value = data_bytes.get(entry_addr, 0)
            
            # Offset of this GOT entry within the .data section
            data_offset = entry_addr - data_base
            
            # Skip if already has a relocation
            if data_offset in existing_offsets:
                continue
            
            # If the entry contains a valid EBIN-internal address, add a relocation
            if 0 < entry_value <= bss_end:
                relocations.append(Relocation(
                    offset=data_offset,
                    reloc_type=EBIN_RELOC_ABSOLUTE,
                    section=1  # data section
                ))
                got_reloc_count += 1
        
        if got_reloc_count > 0:
            print(f"Synthesized {got_reloc_count} GOT relocation(s)")
    
    return relocations


def build_ebin(config: EbinConfig, code: bytes, data: bytes, bss_size: int,
               entry_offset: int, relocations: List[Relocation]) -> bytes:
    """Build EBIN file from components"""
    # Calculate offsets
    header_size = 60  # ebin_header_t is 60 bytes
    reloc_size = len(relocations) * 8
    code_offset = header_size + reloc_size
    data_offset = code_offset + len(code)
    
    # Pad to 4-byte alignment
    if len(code) % 4 != 0:
        code = code + b'\x00' * (4 - len(code) % 4)
        data_offset = code_offset + len(code)
    
    # Build header (60 bytes - matches ebin_header_t)
    header = struct.pack('<IHHIIIIIIIIIIIII',
        EBIN_MAGIC,                          # magic
        EBIN_VERSION,                        # version
        COMPONENT_TYPES[config.component_type],  # type
        config.flags,                        # flags
        len(code),                           # code_size
        len(data),                           # data_size
        bss_size,                            # bss_size
        entry_offset,                        # entry_offset
        config.interface_version,            # interface_version
        config.min_ram,                      # min_ram
        len(relocations),                    # reloc_count
        header_size,                         # reloc_offset
        code_offset,                         # code_offset
        data_offset,                         # data_offset
        0,                                   # symbol_offset (none)
        0,                                   # symbol_count (none)
    )
    
    assert len(header) == 60, f"Header size mismatch: {len(header)} != 60"
    
    # Build relocation table
    reloc_data = b''
    for r in relocations:
        reloc_data += struct.pack('<IBBH', r.offset, r.reloc_type, r.section, 0)
    
    # Combine all sections
    return header + reloc_data + code + data


def create_linker_script(output_path: str) -> None:
    """Create linker script for EBIN components"""
    script = """/*
 * EBIN Component Linker Script
 * Position-independent code for loading into PSRAM
 */

ENTRY(component_entry)

SECTIONS
{
    . = 0;
    
    .text : {
        *(.text.component_entry)
        *(.text .text.*)
    }
    
    . = ALIGN(4);
    
    .rodata : {
        *(.rodata .rodata.*)
    }
    
    . = ALIGN(4);
    
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    }
    
    . = ALIGN(4);
    
    .got : {
        *(.got)
        *(.got.plt)
    }
    
    . = ALIGN(4);
    
    .bss : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
        *(.debug*)
    }
}
"""
    with open(output_path, 'w') as f:
        f.write(script)


def main():
    parser = argparse.ArgumentParser(description='Build EBIN component files')
    parser.add_argument('sources', nargs='+', help='C source files')
    parser.add_argument('-o', '--output', required=True, help='Output EBIN file')
    parser.add_argument('-t', '--type', choices=COMPONENT_TYPES.keys(), required=True,
                        help='Component type')
    parser.add_argument('-e', '--entry', default='component_entry',
                        help='Entry point symbol (default: component_entry)')
    parser.add_argument('-I', '--include', action='append', default=[],
                        help='Include directory')
    parser.add_argument('-D', '--define', action='append', default=[],
                        help='Preprocessor define')
    parser.add_argument('--interface-version', type=lambda x: int(x, 0), default=0x00010000,
                        help='Interface version (default: 0x00010000)')
    parser.add_argument('--min-ram', type=int, default=0,
                        help='Minimum RAM required')
    parser.add_argument('--debug', action='store_true', help='Debug build')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    # Find toolchain
    try:
        toolchain = find_toolchain()
        print(f"Using toolchain: {toolchain}")
    except RuntimeError as e:
        print(f"Error: {e}")
        sys.exit(1)
    
    # Calculate flags
    flags = 0
    if args.debug:
        flags |= 2  # EBIN_FLAG_DEBUG
    
    # Create configuration
    config = EbinConfig(
        sources=args.sources,
        output=args.output,
        component_type=args.type,
        entry_symbol=args.entry,
        interface_version=args.interface_version,
        min_ram=args.min_ram,
        flags=flags,
        include_dirs=args.include,
        defines=args.define,
        toolchain_prefix=toolchain,
        debug=args.debug,
    )
    
    # Create temp directory for intermediates
    with tempfile.TemporaryDirectory() as tmpdir:
        elf_file = os.path.join(tmpdir, 'component.elf')
        ld_script = os.path.join(tmpdir, 'component.ld')
        
        # Create linker script
        create_linker_script(ld_script)
        
        # Compile sources (one .o per source file)
        obj_files = compile_sources(config, tmpdir)
        
        # Link all object files
        link_object(config, obj_files, elf_file, ld_script)
        
        # Extract sections
        code, data, bss_size = extract_sections(config, elf_file)
        
        # Get entry offset
        entry_offset = get_entry_offset(config, elf_file)
        
        # Extract relocations
        relocations = extract_relocations(config, elf_file)
        
        if args.verbose:
            print(f"Code size: {len(code)} bytes")
            print(f"Data size: {len(data)} bytes")
            print(f"BSS size: {bss_size} bytes")
            print(f"Entry offset: {entry_offset}")
            print(f"Relocations: {len(relocations)}")
    
    # Build EBIN
    ebin_data = build_ebin(config, code, data, bss_size, entry_offset, relocations)
    
    # Write output
    with open(args.output, 'wb') as f:
        f.write(ebin_data)
    
    print(f"Built {args.output}: {len(ebin_data)} bytes")
    print(f"  Code: {len(code)} bytes, Data: {len(data)} bytes, BSS: {bss_size} bytes")
    print(f"  Entry: {config.entry_symbol} @ offset {entry_offset}")
    print(f"  Relocations: {len(relocations)}")


if __name__ == '__main__':
    main()
