#!/bin/sh
# Build Ruby 1.8.6 (v1_8_6_101 checkout expected at private/ruby; excluded by
# gitignore) as a static library on macOS arm64.
# Produces miniruby + libruby-static.a.
#
# What each waiver is for (2026, macOS Golden Gate + Apple clang 21):
#   modern config.guess/config.sub  2007 versions predate aarch64
#   -std=gnu89 -fcommon             K&R definitions, duplicated commons
#   -Wno-int-conversion             VALUE <-> pointer casts (hard error in clang 15+)
#   -Wno-implicit-function-declaration and friends: pre-C99 style throughout
set -e
cd "$(dirname "$0")/../private/ruby"

if [ ! -f configure ]; then
    autoconf # 2.73 works; obsolete-macro warnings are harmless
fi
AUTOMAKE_DIR=$(ls -d /opt/homebrew/share/automake-* 2>/dev/null | head -1)
if [ -n "$AUTOMAKE_DIR" ]; then
    cp "$AUTOMAKE_DIR/config.guess" "$AUTOMAKE_DIR/config.sub" .
fi

CFLAGS="-g -O2 -std=gnu89 -fcommon \
 -Wno-implicit-function-declaration -Wno-incompatible-function-pointer-types \
 -Wno-implicit-int -Wno-deprecated-declarations -Wno-return-type \
 -Wno-int-conversion" ./configure --disable-shared

make miniruby libruby-static.a -j"$(nproc)" # link with: -ldl -lobjc
./miniruby -v
