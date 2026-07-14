include(ExternalProject)
include(ProcessorCount)

set(RUBY186_SOURCE_DIR "${CMAKE_SOURCE_DIR}/vendor/ruby")
set(RUBY186_BINARY_DIR "${CMAKE_BINARY_DIR}/ruby186")

# macOS: use the system make instead of relying on a Homebrew gmake that may not exist
find_program(RUBY186_MAKE_PROGRAM NAMES make gmake
    PATHS /usr/bin /bin
    NO_DEFAULT_PATH
    REQUIRED
)
ProcessorCount(RUBY186_BUILD_JOBS)
if(NOT RUBY186_BUILD_JOBS)
    set(RUBY186_BUILD_JOBS 1)
endif()

# macOS: let Ruby 1.8 compile with modern Clang
set(_ruby186_platform_flags)
foreach(_arch IN LISTS CMAKE_OSX_ARCHITECTURES)
    list(APPEND _ruby186_platform_flags "-arch" "${_arch}")
endforeach()
if(CMAKE_OSX_SYSROOT)
    list(APPEND _ruby186_platform_flags "-isysroot" "${CMAKE_OSX_SYSROOT}")
endif()
if(CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND _ruby186_platform_flags
        "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
string(JOIN " " RUBY186_PLATFORM_FLAGS ${_ruby186_platform_flags})

set(RUBY186_CFLAGS
    "-g -O2 -std=gnu89 -fcommon -Wno-implicit-function-declaration -Wno-incompatible-function-pointer-types -Wno-implicit-int -Wno-deprecated-declarations -Wno-deprecated-non-prototype -Wno-return-type -Wno-int-conversion ${RUBY186_PLATFORM_FLAGS}")
string(STRIP "${RUBY186_CFLAGS}" RUBY186_CFLAGS)
set(RUBY186_LDFLAGS "${RUBY186_PLATFORM_FLAGS}")

set(RUBY186_EXTINIT "${RUBY186_BINARY_DIR}/ext/extinit.o")
set(RUBY186_STRINGIO "${RUBY186_BINARY_DIR}/ext/stringio/stringio.a")
set(RUBY186_SYCK "${RUBY186_BINARY_DIR}/ext/syck/syck.a")
set(RUBY186_LIBRARY "${RUBY186_BINARY_DIR}/libruby-static.a")

ExternalProject_Add(ruby186_build
    SOURCE_DIR "${RUBY186_SOURCE_DIR}"
    BINARY_DIR "${RUBY186_BINARY_DIR}"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    CONFIGURE_COMMAND
        "${CMAKE_COMMAND}" -E env
        "CC=${CMAKE_C_COMPILER}"
        "CFLAGS=${RUBY186_CFLAGS}"
        "LDFLAGS=${RUBY186_LDFLAGS}"
        "<SOURCE_DIR>/configure"
        "--srcdir=<SOURCE_DIR>"
        --disable-shared
    BUILD_COMMAND
        "${RUBY186_MAKE_PROGRAM}" "-j${RUBY186_BUILD_JOBS}"
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
target_link_libraries(opensoup_ruby186 INTERFACE
    "${RUBY186_EXTINIT}"
    "${RUBY186_STRINGIO}"
    "${RUBY186_SYCK}"
    "${RUBY186_LIBRARY}"
    dl
    objc
)
