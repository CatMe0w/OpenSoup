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
endif()

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
        "AR=${CMAKE_AR}"
        "RANLIB=${CMAKE_RANLIB}"
        "CFLAGS=${RUBY186_CFLAGS}"
        "LDFLAGS=${RUBY186_LDFLAGS}"
        "<SOURCE_DIR>/configure"
        "--srcdir=<SOURCE_DIR>"
        --disable-shared
        ${RUBY186_CONFIGURE_PLATFORM_ARGS}
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
    "${RUBY186_SYSTEM_LIBRARIES}"
)
