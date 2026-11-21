cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Sudeki - Static Recompilation
#
# Adapted from xboxrecomp/templates/new-game for a Linux host (SDL2 +
# OpenGL 3.3 via the toolkit's d3d8_gl backend). The Windows path is kept
# behind the same target; the toolkit's own CMake selects the graphics and
# host layers per platform.
# ============================================================================

project(sudeki C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common definitions
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── xboxrecomp toolkit ──────────────────────────────────────────
# Fetched, not vendored: the clone lives in ./xboxrecomp (gitignored) and
# is pinned to a tagged commit recorded in THIRD_PARTY.md.
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

# Builds the runtime libraries and provides the 'xboxrecomp' umbrella target.
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# ── Recompiled game executable ──────────────────────────────────

# Generated code (translated from the guest binary) never enters git; it is
# rebuilt from game/default.xbe by tools.recomp. CONFIGURE_DEPENDS so a
# re-generation that adds or removes chunks is picked up without a manual
# re-configure.
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")

set(GAME_SOURCES
    src/main.c
    src/recomp_manual.c
    # Add game-specific sources here as bring-up proceeds.
)

add_executable(${PROJECT_NAME}
    ${GAME_SOURCES}
    ${RECOMP_GEN_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)

# Include paths for generated recomp headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${XBOXRECOMP_DIR}/include
)

if(WIN32)
    # Windows SDK libraries needed at link time
    target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d11
        dxgi
        dxguid
        $<IF:$<BOOL:${MINGW}>,xinput1_4,xinput>
        winmm
        dbghelp
    )
    if(MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE /LARGEADDRESSAWARE)
        target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
    endif()
else()
    # POSIX host: libm for the x87/SSE helpers, libdl for dladdr in the
    # crash handler (it names the guest function a fault happened in), and
    # -rdynamic so dladdr can actually see the generated sub_* symbols.
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl)
    target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
endif()

# ── Game-specific additions ─────────────────────────────────────
