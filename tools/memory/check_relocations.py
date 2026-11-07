#!/usr/bin/env python3
"""Reject memory packages that would execute unrelocated addresses on Xtensa.

Accept a release ZIP or its uncompressed DMF. Only the standard library is used.
The offsets below describe the 32-bit DMF header (dmod_types.h); fail closed if
that format changes rather than silently approving an unchecked package.
"""
import argparse
from pathlib import Path
import struct
import zipfile


def check(data):
    if len(data) < 240:
        raise ValueError("truncated DMF header")
    signature, header_size, _, pointer_size = struct.unpack_from("<4I", data)
    if signature != 0x444D4F44 or header_size != 240 or pointer_size != 4:
        raise ValueError("expected a 32-bit DMF header of 240 bytes")
    arch = data[16:48].split(b"\0", 1)[0]
    if not arch.startswith(b"xtensa"):
        raise ValueError("expected an Xtensa module")
    footer = struct.unpack_from("<Q", data, 224)[0]
    if footer < header_size or footer > len(data) - 64:
        raise ValueError("truncated module footer")
    start, size = struct.unpack_from("<II", data, footer + 6 * 8)
    if size < 8 or size % 4 or start < header_size or start > len(data) - size:
        raise ValueError("missing relocation table: rebuild memory with current dmod and DMOD_USE_RELDYN")
    words = struct.unpack_from(f"<{size // 4}I", data, start)
    for i in range(len(words) - 1):
        if words[i] == 1 and 0 < words[i + 1] <= len(words) - i - 2:
            offsets = words[i + 2:i + 2 + words[i + 1]]
            if any(offset % 4 or offset > start - 4 for offset in offsets):
                raise ValueError("invalid relocation offset")
            return len(offsets)
    raise ValueError("missing nonempty relocation table: rebuild memory with current dmod")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    args = parser.parse_args()
    try:
        if args.module.suffix == ".zip":
            with zipfile.ZipFile(args.module) as package:
                data = package.read("memory.dmf")
        else:
            data = args.module.read_bytes()
        count = check(data)
    except (OSError, ValueError, KeyError, struct.error, zipfile.BadZipFile) as error:
        parser.exit(1, f"memory: {error}\n")
    print(f"memory: validated {count} Xtensa relocations")


if __name__ == "__main__":
    main()
