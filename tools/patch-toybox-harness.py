#!/usr/bin/env python3
import hashlib
import struct
import sys
from pathlib import Path


SOURCE_SHA256 = "74e615ce649780c32e79655edc7f74e70ae4ab14e92438c0e9cfa86b03b2893b"

TEXT_VA, TEXT_RAW = 0x401000, 0x400
# 507 file-backed bytes past .text's last instruction (vsize 0x2B4E05, raw size 0x2B5000): executable, and free
CAVE_VA = 0x6B5E08

# Paths come from the environment because the engine chdirs during boot
SCRIPT_ENV = rb"TOYBOX_HARNESS_IN"
OUTPUT_ENV = rb"TOYBOX_HARNESS_OUT"

IAT_CREATE_FILE_A = 0x6B60FC
IAT_READ_FILE = 0x6B6224
IAT_SLEEP = 0x6B60C4
IAT_GET_ENV = 0x6B6098
CRT_IOB = 0x45E3BD    # returns &__iob[0]; stdout is +0x20
CRT_FREOPEN = 0x5FED41
STR_W = 0x6FF96C

CTOR_STD_HANDLES = (0x4F3F89, 0x4F3FEF)  # Souptoys_Console__ctor @0x4F3F40, @0x4F3FA0
CTOR_FREOPEN = (0x4F3F68, 0x4F3FCE)      # their freopen("CON", "w", stdout)
READ_CALL_SETUP = 0x4F41FD               # Souptoys_Console__vf0's read loop


def va_to_file(va):
    return va - TEXT_VA + TEXT_RAW


def rel32(source_end, target):
    return struct.pack("<i", target - source_end)


# strings first, so every code address falls out of the bytes below
SCRIPT_ENV_VA = CAVE_VA
OUTPUT_ENV_VA = SCRIPT_ENV_VA + len(SCRIPT_ENV) + 1
OPEN_SCRIPT_VA = OUTPUT_ENV_VA + len(OUTPUT_ENV) + 1

# leaves a 260-byte stack buffer holding the variable, or "" if it is unset
def read_env(name_va):
    return (
        b"\x81\xec\x04\x01\x00\x00"              # sub esp, 104h
        b"\xc6\x04\x24\x00"                      # mov byte ptr [esp], 0
        b"\x68\x04\x01\x00\x00"                  # push 104h
        b"\x8d\x44\x24\x04"                      # lea eax, [esp+4]
        b"\x50"                                  # push eax
        + b"\x68" + struct.pack("<I", name_va)
        + b"\xff\x15" + struct.pack("<I", IAT_GET_ENV)
    )


# console->input_handle = CreateFileA($TOYBOX_HARNESS_IN, ...), `this` in esi
OPEN_SCRIPT = (
    read_env(SCRIPT_ENV_VA)
    + b"\x6a\x00"                                # push 0         hTemplateFile
    b"\x6a\x00"                                  # push 0         dwFlagsAndAttributes
    b"\x6a\x03"                                  # push 3         OPEN_EXISTING
    b"\x6a\x00"                                  # push 0         lpSecurityAttributes
    b"\x6a\x03"                                  # push 3         FILE_SHARE_READ|WRITE
    b"\x68\x00\x00\x00\x80"                      # push 80000000h GENERIC_READ
    b"\x8d\x44\x24\x18"                          # lea eax, [esp+18h]  the buffer
    b"\x50"                                      # push eax
    + b"\xff\x15" + struct.pack("<I", IAT_CREATE_FILE_A)
    + b"\x89\x46\x25"                            # mov [esi+25h], eax
    b"\x81\xc4\x04\x01\x00\x00"                  # add esp, 104h
    b"\xc3"
)

# ReadFile with the console's own arguments, polling at end of script. Ending
# the process there would not do: a second thread pumps the same console and
# gets there while the first is still running a scenario.
READ_LINE = (
    b"\x55"                                      # push ebp
    b"\x8b\xec"                                  # mov ebp, esp
    b"\x53"                                      # push ebx
    b"\x8b\x5d\x14"                              # mov ebx, [ebp+14h]   lpNumberOfBytesRead
    b"\xff\x75\x18"                              # retry: push [ebp+18h] lpOverlapped
    b"\x53"                                      # push ebx
    b"\xff\x75\x10"                              # push [ebp+10h]       nNumberOfBytesToRead
    b"\xff\x75\x0c"                              # push [ebp+0Ch]       lpBuffer
    b"\xff\x75\x08"                              # push [ebp+08h]       hFile
    + b"\xff\x15" + struct.pack("<I", IAT_READ_FILE)
    + b"\x83\x3b\x00"                            # cmp dword ptr [ebx], 0
    b"\x75\x0a"                                  # jnz short done
    b"\x6a\x14"                                  # push 20
    + b"\xff\x15" + struct.pack("<I", IAT_SLEEP)
    + b"\xeb\xde"                                # jmp retry
    b"\x5b"                                      # done: pop ebx
    b"\x5d"                                      # pop ebp
    b"\xc2\x14\x00"                              # retn 14h
)

READ_LINE_VA = OPEN_SCRIPT_VA + len(OPEN_SCRIPT)
OPEN_OUTPUT_VA = READ_LINE_VA + len(READ_LINE)

# freopen($TOYBOX_HARNESS_OUT, "w", stdout), replacing the ctor's freopen("CON")
OPEN_OUTPUT = (
    read_env(OUTPUT_ENV_VA)
    + b"\xe8" + rel32(OPEN_OUTPUT_VA + len(read_env(0)) + 5, CRT_IOB)
    + b"\x83\xc0\x20"                            # add eax, 20h        stdout
    b"\x50"                                      # push eax
    + b"\x68" + struct.pack("<I", STR_W)
    + b"\x8d\x44\x24\x08"                        # lea eax, [esp+8]    the buffer
    b"\x50"                                      # push eax
    + b"\xe8" + rel32(OPEN_OUTPUT_VA + len(read_env(0)) + 24, CRT_FREOPEN)
    + b"\x83\xc4\x0c"                            # add esp, 0Ch
    b"\x81\xc4\x04\x01\x00\x00"                  # add esp, 104h
    b"\xc3"
)

CAVE = (SCRIPT_ENV + b"\0" + OUTPUT_ENV + b"\0"
        + OPEN_SCRIPT + READ_LINE + OPEN_OUTPUT)

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
    # File 0x269132 / VA 0x669D32: both CreateMutexA guards -> nops, allowing multiple instances to run at once
    (
        bytes.fromhex("3d b7 00 00 00 74 17 ff 15 14 63 6b 00 83 f8 05 74 0c"),
        bytes.fromhex("3d b7 00 00 00 90 90 ff 15 14 63 6b 00 83 f8 05 90 90"),
    ),
    # File 0x1958E2 / VA 0x596AE2: jnz -> jmp, allowing unsigned .toy
    (
        bytes.fromhex("0f b6 55 f3 85 d2 75 4e 0f b6 45 10 85 c0 75 46"),
        bytes.fromhex("0f b6 55 f3 85 d2 eb 4e 0f b6 45 10 85 c0 75 46"),
    ),
    # VA 0x4F3F57 / 0x4F3FBD: drop AllocConsole, so no console window appears
    (
        bytes.fromhex("c7 06 18 16 6d 00 89 46 2d 89 46 31 ff 15 08 63 6b 00"),
        bytes.fromhex("c7 06 18 16 6d 00 89 46 2d 89 46 31 90 90 90 90 90 90"),
    ),
    (
        bytes.fromhex("c7 46 2d 00 00 00 00 89 46 31 ff 15 08 63 6b 00"),
        bytes.fromhex("c7 46 2d 00 00 00 00 89 46 31 90 90 90 90 90 90"),
    ),
    # VA 0x4F40EB: WriteConsoleA -> WriteFile, for the prompt on the inherited stdout
    (
        bytes.fromhex("8b 41 29 50 ff 15 38 62 6b 00"),
        bytes.fromhex("8b 41 29 50 ff 15 a8 61 6b 00"),
    ),
)

# Same bytes at both ctors, and the replacement is site-relative, so these go by address rather than by pattern
PATCHES = (
    # VA 0x4F3F89 / 0x4F3FEF: read the script from a file instead of stdin. The
    # output handle stays on the inherited stdout.
    [(va, b"\xe8" + rel32(va + 5, OPEN_SCRIPT_VA)
          + bytes.fromhex("6a f5 ff d7 89 46 29 90 90"))
     for va in CTOR_STD_HANDLES]
    # VA 0x4F3F68 / 0x4F3FCE: freopen("CON") -> the thunk. Keeps the mov edi,
    # drops the add esp,0Ch that balanced the original's cdecl call.
    + [(va, b"\xe8" + rel32(va + 5, OPEN_OUTPUT_VA) + b"\x90" * 19
            + bytes.fromhex("8b 3d ac 61 6b 00") + b"\x90" * 3)
       for va in CTOR_FREOPEN]
    # VA 0x4F41FD: mov edi, ds:ReadConsoleA -> mov edi, <the ReadFile thunk>
    + [(READ_CALL_SETUP, b"\xbf" + struct.pack("<I", READ_LINE_VA) + b"\x90")]
)


if len(sys.argv) != 3:
    raise SystemExit(f"usage: {sys.argv[0]} INPUT_Toybox.exe OUTPUT_Toybox.exe")

source, output = map(Path, sys.argv[1:])
data = source.read_bytes()
digest = hashlib.sha256(data).hexdigest()
if digest != SOURCE_SHA256:
    raise SystemExit(f"wrong input SHA-256: {digest}")

data = bytearray(data)
for before, after in REPLACEMENTS:
    data[:] = data.replace(before, after, 1)
for va, new in PATCHES:
    data[va_to_file(va):va_to_file(va) + len(new)] = new
data[va_to_file(CAVE_VA):va_to_file(CAVE_VA) + len(CAVE)] = CAVE

output.write_bytes(data)
print(hashlib.sha256(data).hexdigest(), output)
