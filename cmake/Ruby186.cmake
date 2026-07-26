include(ExternalProject)
include(ProcessorCount)

set(RUBY186_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/ruby")
set(RUBY186_BINARY_DIR "${CMAKE_BINARY_DIR}/ruby186")

set(RUBY186_CONFIGURE_PLATFORM_ARGS)
set(_ruby186_platform_flags)
if(APPLE)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _ruby186_arch_count)
    if(_ruby186_arch_count GREATER 1)
        message(FATAL_ERROR
            "Ruby 1.8.6 must be built for one macOS architecture per build tree")
    endif()
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" AND
       CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        list(APPEND RUBY186_CONFIGURE_PLATFORM_ARGS
            --build=x86_64-apple-darwin
            --host=x86_64-apple-darwin
        )
    endif()
    foreach(_arch IN LISTS CMAKE_OSX_ARCHITECTURES)
        list(APPEND _ruby186_platform_flags "-arch" "${_arch}")
    endforeach()
    if(CMAKE_OSX_SYSROOT)
        list(APPEND _ruby186_platform_flags
            "-isysroot" "${CMAKE_OSX_SYSROOT}")
    endif()
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND _ruby186_platform_flags
            "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
endif()
if(MINGW)
    find_program(RUBY186_SHELL NAMES sh REQUIRED)
    find_program(RUBY186_AS NAMES as REQUIRED)
    find_program(RUBY186_NM NAMES nm REQUIRED)
    find_program(RUBY186_WINDRES NAMES windres REQUIRED)
    find_program(RUBY186_DLLWRAP NAMES dllwrap REQUIRED)
    find_program(RUBY186_OBJDUMP NAMES objdump REQUIRED)
    find_program(RUBY186_GREP NAMES grep REQUIRED)
    list(APPEND RUBY186_CONFIGURE_PLATFORM_ARGS
        --build=i686-w64-mingw32
        --host=i686-w64-mingw32
        --with-winsock2
    )
endif()

if(APPLE)
    # Do not rely on a Homebrew gmake that may not exist.
    find_program(RUBY186_MAKE_PROGRAM NAMES make gmake
        PATHS /usr/bin /bin
        NO_DEFAULT_PATH
        REQUIRED
    )
else()
    find_program(RUBY186_MAKE_PROGRAM NAMES make gmake REQUIRED)
endif()
ProcessorCount(RUBY186_BUILD_JOBS)
if(NOT RUBY186_BUILD_JOBS)
    set(RUBY186_BUILD_JOBS 1)
endif()

# let Ruby 1.8 compile with modern C compilers
set(_ruby186_compat_flags
    -g -O2 -std=gnu89 -fcommon
    -Wno-implicit-function-declaration
    -Wno-implicit-int
    -Wno-deprecated-declarations
    -Wno-return-type
    -Wno-int-conversion
)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    list(APPEND _ruby186_compat_flags
        -Wno-incompatible-function-pointer-types
        -Wno-deprecated-non-prototype
    )
endif()
list(APPEND _ruby186_compat_flags ${_ruby186_platform_flags})
string(JOIN " " RUBY186_CFLAGS ${_ruby186_compat_flags})
string(JOIN " " RUBY186_LDFLAGS ${_ruby186_platform_flags})

set(RUBY186_SYSTEM_LIBRARIES "${CMAKE_DL_LIBS}")
if(APPLE)
    list(APPEND RUBY186_SYSTEM_LIBRARIES objc)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(RUBY186_CRYPT_LIBRARY NAMES crypt REQUIRED)
    list(APPEND RUBY186_SYSTEM_LIBRARIES
        "${RUBY186_CRYPT_LIBRARY}"
        m
    )
elseif(MINGW)
    list(APPEND RUBY186_SYSTEM_LIBRARIES ws2_32 m)
endif()

set(RUBY186_EXTINIT "${RUBY186_BINARY_DIR}/ext/extinit.o")
set(RUBY186_STRINGIO "${RUBY186_BINARY_DIR}/ext/stringio/stringio.a")
set(RUBY186_SYCK "${RUBY186_BINARY_DIR}/ext/syck/syck.a")
if(MINGW)
    set(RUBY186_LIBRARY
        "${RUBY186_BINARY_DIR}/libmsvcrt-ruby18-static.a")
else()
    set(RUBY186_LIBRARY "${RUBY186_BINARY_DIR}/libruby-static.a")
endif()

set(RUBY186_CONFIGURE_COMMAND "<SOURCE_DIR>/configure")
if(MINGW)
    set(RUBY186_CONFIGURE_COMMAND
        "${RUBY186_SHELL}" "<SOURCE_DIR>/configure")
endif()

set(RUBY186_CONFIGURE_ENVIRONMENT
    "CC=${CMAKE_C_COMPILER}"
    "AR=${CMAKE_AR}"
    "RANLIB=${CMAKE_RANLIB}"
    "CFLAGS=${RUBY186_CFLAGS}"
    "LDFLAGS=${RUBY186_LDFLAGS}"
)
if(MINGW)
    # Busybox sh on Windows cannot reliably infer these from PATH, and
    # these ABI definitions must be visible before any MinGW system header.
    list(APPEND RUBY186_CONFIGURE_ENVIRONMENT
        "PATH_SEPARATOR=$<SEMICOLON>"
        "AS=${RUBY186_AS}"
        "NM=${RUBY186_NM}"
        "WINDRES=${RUBY186_WINDRES}"
        "DLLWRAP=${RUBY186_DLLWRAP}"
        "OBJDUMP=${RUBY186_OBJDUMP}"
        "GREP=${RUBY186_GREP}"
        "EGREP=${RUBY186_GREP} -E"
        "CPPFLAGS=-D_FILE_OFFSET_BITS=64 -D_TIMEZONE_DEFINED"
    )
endif()

set(RUBY186_BUILD_COMMAND
    "${RUBY186_MAKE_PROGRAM}" "-j${RUBY186_BUILD_JOBS}"
)
if(MINGW)
    # Ruby 1.8's MinGW GNUmakefile uses dllwrap even for a static build.
    # Build the core in parallel, then integrate only the required static
    # extensions serially to avoid its extinit.o link race.
    set(RUBY186_BUILD_COMMAND
        "${CMAKE_COMMAND}" -E touch_nocreate "<SOURCE_DIR>/parse.c"
        COMMAND
        "${RUBY186_MAKE_PROGRAM}" "-j${RUBY186_BUILD_JOBS}"
        "RUBY_EXP="
        "LIBRUBY=libmsvcrt-ruby18-static.a"
        "LIBRUBYARG=-lmsvcrt-ruby18-static"
        miniruby.exe .rbconfig.time
        COMMAND
        "${RUBY186_MAKE_PROGRAM}" -j1
        "RUBY_EXP="
        "LIBRUBY=libmsvcrt-ruby18-static.a"
        "LIBRUBYARG=-lmsvcrt-ruby18-static"
        "EXTSTATIC=stringio,syck"
        "EXTS=stringio,syck"
    )
endif()

ExternalProject_Add(ruby186_build
    SOURCE_DIR "${RUBY186_SOURCE_DIR}"
    BINARY_DIR "${RUBY186_BINARY_DIR}"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}" -E env
        ${RUBY186_CONFIGURE_ENVIRONMENT}
        ${RUBY186_CONFIGURE_COMMAND}
        "--srcdir=<SOURCE_DIR>"
        --disable-shared
        ${RUBY186_CONFIGURE_PLATFORM_ARGS}
    BUILD_COMMAND ${RUBY186_BUILD_COMMAND}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS
        "${RUBY186_BINARY_DIR}/config.h"
        "${RUBY186_EXTINIT}"
        "${RUBY186_STRINGIO}"
        "${RUBY186_SYCK}"
        "${RUBY186_LIBRARY}"
)

add_library(opensoup_ruby186 INTERFACE)
add_library(Ruby186::Embed ALIAS opensoup_ruby186)
add_dependencies(opensoup_ruby186 ruby186_build)
target_include_directories(opensoup_ruby186 INTERFACE
    "${RUBY186_SOURCE_DIR}"
    "${RUBY186_BINARY_DIR}"
)
# Ruby 1.8 uses empty parameter lists for ANYARGS, in C23 those mean (void)
target_compile_options(opensoup_ruby186 INTERFACE
    "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-std=gnu17>"
)
if(MINGW)
    target_compile_definitions(opensoup_ruby186 INTERFACE
        _FILE_OFFSET_BITS=64
        _TIMEZONE_DEFINED
    )
endif()
target_link_libraries(opensoup_ruby186 INTERFACE
    "${RUBY186_EXTINIT}"
    "${RUBY186_STRINGIO}"
    "${RUBY186_SYCK}"
    "${RUBY186_LIBRARY}"
    "${RUBY186_SYSTEM_LIBRARIES}"
)
