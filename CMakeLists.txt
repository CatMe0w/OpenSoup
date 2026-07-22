cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND NOT CMAKE_TOOLCHAIN_FILE)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR
       (NOT CMAKE_OSX_ARCHITECTURES AND
        CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64"))
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING
            "Minimum supported macOS version")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum supported macOS version")
    endif()
endif()

project(opensoup VERSION 0.1.0 LANGUAGES C)

# emit compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

set(SOKOL_DIR "${CMAKE_SOURCE_DIR}/vendor/sokol")
set(CJSON_DIR "${CMAKE_SOURCE_DIR}/vendor/cjson")
set(MINIAUDIO_DIR "${CMAKE_SOURCE_DIR}/vendor/miniaudio")
set(STB_DIR "${CMAKE_SOURCE_DIR}/vendor/stb")

# stb_vorbis 1.22 contains a legacy pointer-wrap guard diagnosed by Clang
set_source_files_properties(src/audio.c PROPERTIES
    COMPILE_OPTIONS "-Wno-tautological-compare"
)

if(APPLE)
    enable_language(OBJC)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Ruby186.cmake")

    add_executable(opensoup MACOSX_BUNDLE
        src/opensoup.c
        src/scene.c
        src/assets.c
        src/sprite_hook.c
        src/toybox.c
        src/toybox_button.c
        src/toybox_scroll.c
        src/physics.c
        src/toyfile.c
        src/toyfile_fs.c
        src/toydefs.c
        src/audio.c
        src/rubyhost.c
        src/rubyhost_engine.c
        src/rubyhost_limb.c
        src/rubyhost_node.c
        src/rubyhost_realize.c
        src/rubyhost_sound.c
        vendor/cjson/cJSON.c
        src/macos/app_paths.m
        src/macos/main_macos.m
        src/macos/sokol_impl.m
    )
    target_include_directories(opensoup PRIVATE
        "${SOKOL_DIR}" "${CJSON_DIR}" "${MINIAUDIO_DIR}" "${STB_DIR}" src)
    set_source_files_properties(
        src/macos/app_paths.m
        src/macos/main_macos.m
        PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc"
    )
    set_source_files_properties(src/rubyhost.c PROPERTIES
        COMPILE_OPTIONS "${RUBYHOST_FLAGS}"
    )
    target_link_libraries(opensoup PRIVATE
        Ruby186::Embed
        "-framework Cocoa"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
    )
    target_link_options(opensoup PRIVATE "-Wl,-dead_strip")
    target_compile_options(opensoup PRIVATE -Wall -Wextra)
    set_target_properties(opensoup PROPERTIES
        OUTPUT_NAME "OpenSoup"
        MACOSX_BUNDLE_BUNDLE_NAME "OpenSoup"
        MACOSX_BUNDLE_GUI_IDENTIFIER "cat.me0w.opensoup"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
    install(TARGETS opensoup BUNDLE DESTINATION ".")

    # headless framework-boot check (no window; run from the repo root)
    add_executable(opensoup_rubyboot
        src/rubyboot_main.c
        src/assets.c
        src/rubyhost.c
        src/rubyhost_engine.c
        src/rubyhost_limb.c
        src/rubyhost_node.c
        src/rubyhost_realize.c
        src/rubyhost_sound.c
        src/toybox_button.c
        src/toybox_scroll.c
        src/physics.c
        src/toyfile.c
        src/toyfile_fs.c
        src/toydefs.c
        src/audio.c
        vendor/cjson/cJSON.c
    )
    target_include_directories(opensoup_rubyboot PRIVATE
        "${CJSON_DIR}" "${MINIAUDIO_DIR}" "${STB_DIR}" src)
    target_link_libraries(opensoup_rubyboot PRIVATE
        Ruby186::Embed
        "-framework AudioToolbox"
        "-framework CoreAudio"
    )
    target_compile_options(opensoup_rubyboot PRIVATE -Wall -Wextra)
else()
    message(FATAL_ERROR "only the macOS application exists so far")
endif()

set(OPENSOUP_PACKAGE_ARCH "${CMAKE_OSX_ARCHITECTURES}")
if(NOT OPENSOUP_PACKAGE_ARCH)
    set(OPENSOUP_PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_GENERATOR "DragNDrop")
set(CPACK_PACKAGE_NAME "OpenSoup")
set(CPACK_PACKAGE_VENDOR "OpenSoup")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "An open-source remake of Souptoys Toybox")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "OpenSoup-${PROJECT_VERSION}-macOS-${OPENSOUP_PACKAGE_ARCH}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/package")
set(CPACK_DMG_VOLUME_NAME "OpenSoup ${PROJECT_VERSION}")
set(CPACK_DMG_FORMAT "UDZO")
set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
include(CPack)
