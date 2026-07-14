# Build x86_64 binaries on an Apple Silicon host. Ruby's x86_64 miniruby
# cannot invoke the /usr/bin xcrun shims under Rosetta, so use the underlying
# Apple toolchain executables and an explicit SDK path.
set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "")
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING
        "Minimum supported macOS version")
endif()

execute_process(
    COMMAND /usr/bin/xcrun --sdk macosx --find clang
    OUTPUT_VARIABLE _clang
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
get_filename_component(_toolchain_bin "${_clang}" DIRECTORY)

execute_process(
    COMMAND /usr/bin/xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE _sdk
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

set(CMAKE_C_COMPILER "${_clang}" CACHE FILEPATH "")
set(CMAKE_OBJC_COMPILER "${_clang}" CACHE FILEPATH "")
set(CMAKE_AR "${_toolchain_bin}/ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${_toolchain_bin}/ranlib" CACHE FILEPATH "")
set(CMAKE_OSX_SYSROOT "${_sdk}" CACHE PATH "")
