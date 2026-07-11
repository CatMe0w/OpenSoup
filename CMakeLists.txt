cmake_minimum_required(VERSION 3.21)
project(opensoup C)

# emit compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

set(SOKOL_DIR "${CMAKE_SOURCE_DIR}/vendor/sokol")
set(CJSON_DIR "${CMAKE_SOURCE_DIR}/vendor/cjson")

if(APPLE)
    enable_language(OBJC)
    add_executable(opensoup_mvp
        src/scene.c
        src/assets.c
        src/demo.c
        src/physics.c
        src/toydefs.c
        vendor/cjson/cJSON.c
        src/macos/main_macos.m
        src/macos/sokol_impl.m
    )
    target_include_directories(opensoup_mvp PRIVATE "${SOKOL_DIR}" "${CJSON_DIR}" src)
    set_source_files_properties(src/macos/main_macos.m PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc"
    )
    target_link_libraries(opensoup_mvp PRIVATE
        "-framework Cocoa"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
    )
    target_link_options(opensoup_mvp PRIVATE "-Wl,-dead_strip")
    target_compile_options(opensoup_mvp PRIVATE -Wall -Wextra)
else()
    message(FATAL_ERROR "only the macOS MVP exists so far")
endif()
