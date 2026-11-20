cmake_minimum_required(VERSION 3.21)

project(SkyrimOutfitSystemNG VERSION 1.5.39 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT: the shipped DLL must not require MSVCP140/VCRUNTIME140(_1).
# SKSE loads plugins with a plain LoadLibrary, so a missing runtime module
# surfaces as "Error 126" with no other diagnostics.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------------------
# CommonLibSSE-NG options
# ---------------------------------------------------------------------------
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(COMMONLIB_ENABLE_IPO OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/CommonLibSSE-NG EXCLUDE_FROM_ALL)
include("${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake")

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE SOS_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_commonlibsse_plugin(SkyrimOutfitSystemNG
    AUTHOR "Skyrim Outfit System SE/NG contributors"
    SOURCES ${SOS_SOURCES} ${SOS_HEADERS}
)

# The version resource is not picked up by the *.cpp glob above; without it the
# DLL has no FileVersion/ProductName at all.
if(WIN32)
    target_sources(SkyrimOutfitSystemNG PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/version.rc"
    )
endif()

target_compile_features(SkyrimOutfitSystemNG PRIVATE cxx_std_20)
target_precompile_headers(SkyrimOutfitSystemNG PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/include/SOS_PCH.h")

target_include_directories(SkyrimOutfitSystemNG PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/include/imgui"
)

target_compile_options(SkyrimOutfitSystemNG PRIVATE
    /utf-8 /Zc:preprocessor /permissive-
    /wd4200 /wd4244 /wd4267
)

target_compile_definitions(SkyrimOutfitSystemNG PRIVATE
    XBYAK_NO_OP_NAMES
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

# ---------------------------------------------------------------------------
# Module file name
# ---------------------------------------------------------------------------
# SKSE hands interface exchange broadcasts only to the plugins that were already
# registered when the sender registered its own listener, so an early module name
# matters for reaching skee64 (see docs/RACEMENU_V4_AUDIT.md). The plugin identity
# stays SkyrimOutfitSystemNG: the log file name, the Papyrus translation prefix and
# the save facing name are built from Plugin::NAME and must not move. Only the file
# on disk changes, and the old name has to stay out of Data/SKSE/Plugins so the
# game never loads two copies.
set_target_properties(SkyrimOutfitSystemNG PROPERTIES OUTPUT_NAME "OutfitSystemNG")

# ---------------------------------------------------------------------------
# Deploy
# ---------------------------------------------------------------------------
add_custom_command(TARGET SkyrimOutfitSystemNG POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/dist/SKSE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SkyrimOutfitSystemNG>" "${CMAKE_CURRENT_SOURCE_DIR}/dist/SKSE/Plugins/OutfitSystemNG.dll"
    COMMENT "Deploying OutfitSystemNG.dll to dist/SKSE/Plugins"
)
