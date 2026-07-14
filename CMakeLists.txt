cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum supported macOS version")
endif()

project(opensoup C)

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

    add_executable(opensoup_mvp
        src/scene.c
        src/assets.c
        src/demo.c
        src/toybox.c
        src/toybox_button.c
        src/toybox_scroll.c
        src/physics.c
        src/toydefs.c
        src/toyphys.c
        src/audio.c
        src/rubyhost.c
        vendor/cjson/cJSON.c
        src/macos/main_macos.m
        src/macos/sokol_impl.m
    )
    target_include_directories(opensoup_mvp PRIVATE
        "${SOKOL_DIR}" "${CJSON_DIR}" "${MINIAUDIO_DIR}" "${STB_DIR}" src)
    set_source_files_properties(src/macos/main_macos.m PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc"
    )
    set_source_files_properties(src/rubyhost.c PROPERTIES
        COMPILE_OPTIONS "${RUBYHOST_FLAGS}"
    )
    target_link_libraries(opensoup_mvp PRIVATE
        Ruby186::Embed
        "-framework Cocoa"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
    )
    target_link_options(opensoup_mvp PRIVATE "-Wl,-dead_strip")
    target_compile_options(opensoup_mvp PRIVATE -Wall -Wextra)

    # headless framework-boot check (no window; run from the repo root)
    add_executable(opensoup_rubyboot
        src/rubyboot_main.c
        src/assets.c
        src/rubyhost.c
        src/toybox_button.c
        src/toybox_scroll.c
        src/physics.c
        src/toydefs.c
        src/toyphys.c
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
    message(FATAL_ERROR "only the macOS MVP exists so far")
endif()
