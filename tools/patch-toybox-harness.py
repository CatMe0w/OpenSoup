#!/usr/bin/env python3
import hashlib
from pathlib import Path
import sys


SOURCE_SHA256 = "74e615ce649780c32e79655edc7f74e70ae4ab14e92438c0e9cfa86b03b2893b"

REPLACEMENTS = (
    # File 0x184: PE Subsystem 2 (GUI) -> 3 (CUI), so Windows creates/attaches a console.
    (
        bytes.fromhex("00 80 37 00 00 04 00 00 25 dc 35 00 02 00 00 80"),
        bytes.fromhex("00 80 37 00 00 04 00 00 25 dc 35 00 03 00 00 80"),
    ),
    # File 0x265BF7 / VA 0x6667F7: add edx,1 -> mov dl,1; nop, forcing the original null-renderer mode.
    (
        bytes.fromhex("0f b6 d0 f7 da 1b d2 83 c2 01 88 95 36 ff ff ff"),
        bytes.fromhex("0f b6 d0 f7 da 1b d2 b2 01 90 88 95 36 ff ff ff"),
    ),
    # File 0x265C26 / VA 0x666826: both developer/CLI guards -> nops, forcing the native developer console.
    (
        bytes.fromhex("74 5e 0f b6 8d 36 ff ff ff 85 c9 75 53 6a 38"),
        bytes.fromhex("90 90 0f b6 8d 36 ff ff ff 85 c9 90 90 6a 38"),
    ),
    # File 0x267526 / VA 0x668126: both developer/CLI guards -> nops, forcing the Ruby developer console thread.
    (
        bytes.fromhex("74 16 0f b6 8d 36 ff ff ff 85 c9 75 0b 8b 8d 58 fc ff ff"),
        bytes.fromhex("90 90 0f b6 8d 36 ff ff ff 85 c9 90 90 8b 8d 58 fc ff ff"),
    ),
    # File 0x2684D6 / VA 0x6690D6: jnz 0x6692AA -> six nops, retaining the normal GetMessage loop.
    (
        bytes.fromhex("0f b6 85 36 ff ff ff 85 c0 0f 85 ce 01 00 00 6a 00"),
        bytes.fromhex("0f b6 85 36 ff ff ff 85 c0 90 90 90 90 90 90 6a 00"),
    ),
)


if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} INPUT_Toybox.exe OUTPUT_Toybox.exe")

source, output = map(Path, sys.argv[1:])
data = source.read_bytes()
digest = hashlib.sha256(data).hexdigest()
if digest != SOURCE_SHA256:
    raise SystemExit(f"wrong input SHA-256: {digest}")

for before, after in REPLACEMENTS:
    data = data.replace(before, after, 1)

output.write_bytes(data)
print(hashlib.sha256(data).hexdigest(), output)
