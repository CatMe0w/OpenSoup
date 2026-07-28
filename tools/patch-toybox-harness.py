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
    # File 0x? / VA 0x?: fix "uninitialized constant SoupvidMovie" when running with "-out"
    # FIXME: this throws `COM-Error from source: desc: error:无效指针 context: wcode:.`
    # probably an already dead/trimmed path, always won't work
    (
        bytes.fromhex("55 8B EC 83 EC 60 89 4D A8 68 50 64 70 00"),
        bytes.fromhex("31 C0 C2 0C 00 60 89 4D A8 68 50 64 70 00"),
    ),
    # cont'd
    (
        bytes.fromhex("55 8B EC 83 EC 60 89 4D A8 68 30 64 70 00"),
        bytes.fromhex("31 C0 C2 0C 00 68 30 64 70 00 90 90 90 90"),
    ),
    # File 0x1958E2 / VA 0x596AE2: jnz -> jmp, allowing unsigned .toy
    (
        bytes.fromhex("0f b6 55 f3 85 d2 75 4e 0f b6 45 10 85 c0 75 46"),
        bytes.fromhex("0f b6 55 f3 85 d2 eb 4e 0f b6 45 10 85 c0 75 46"),
    ),
    # VA 0x4F3F57 / 0x4F3FBD: drop AllocConsole, preserving redirected standard handles.
    (
        bytes.fromhex("c7 06 18 16 6d 00 89 46 2d 89 46 31 ff 15 08 63 6b 00"),
        bytes.fromhex("c7 06 18 16 6d 00 89 46 2d 89 46 31 90 90 90 90 90 90"),
    ),
    (
        bytes.fromhex("c7 46 2d 00 00 00 00 89 46 31 ff 15 08 63 6b 00"),
        bytes.fromhex("c7 46 2d 00 00 00 00 89 46 31 90 90 90 90 90 90"),
    ),
    # VA 0x4F41FD: ReadConsoleA -> ReadFile, allowing redirected stdin.
    (
        bytes.fromhex("8b 3d 00 63 6b 00 89 9c 24 e0 00 00 00"),
        bytes.fromhex("8b 3d 24 62 6b 00 89 9c 24 e0 00 00 00"),
    ),
    # VA 0x4F40EB: WriteConsoleA -> WriteFile, allowing redirected stdout.
    (
        bytes.fromhex("8b 41 29 50 ff 15 38 62 6b 00"),
        bytes.fromhex("8b 41 29 50 ff 15 a8 61 6b 00"),
    ),
    # VA 0x4F3F68 / 0x4F3FCE: drop freopen("CONOUT$") so Ruby keeps redirected stdout.
    (
        bytes.fromhex("e8 50 a4 f6 ff 83 c0 20 50 68 6c f9 6f 00 68 fc 15 6d 00"
                      "e8 c1 ad 10 00 8b 3d ac 61 6b 00 83 c4 0c"),
        bytes.fromhex("90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90"
                      "90 90 90 90 90 8b 3d ac 61 6b 00 90 90 90"),
    ),
    (
        bytes.fromhex("e8 ea a3 f6 ff 83 c0 20 50 68 6c f9 6f 00 68 fc 15 6d 00"
                      "e8 5b ad 10 00 8b 3d ac 61 6b 00 83 c4 0c"),
        bytes.fromhex("90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90"
                      "90 90 90 90 90 8b 3d ac 61 6b 00 90 90 90"),
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
