#
# CMake build for zenoh-flat-c.
#
# Modeled on eclipse-zenoh/zenoh-c's CMakeLists.txt: a hand-rolled
# `add_custom_command` invokes `cargo build` to produce the Rust library and the
# generated flat C header, then IMPORTED targets expose it as `zenohflatc::lib`.
# Trimmed — no SHM/unstable features, no Cargo.toml templating (build.rs already
# runs cbindgen).
#
# The library carries a DUAL API in `include/`:
#   - `zenoh_flat.h`  : the generated flat `z_flat_*` ABI
#   - `zenoh.h`       : the header-only zenoh-c-compatible layer over it
#
cmake_minimum_required(VERSION 3.16)
project(zenoh_flat_c LANGUAGES C)

option(BUILD_SHARED_LIBS "Build/link the shared library (else static)" ON)

enable_testing()

# ── locate cargo output ────────────────────────────────────────────────────
# A single-config generator (Make/Ninja) → cargo profile follows CMAKE_BUILD_TYPE.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR NOT CMAKE_BUILD_TYPE)
    set(cargo_profile_flag "")
    set(cargo_profile_dir "debug")
else()
    set(cargo_profile_flag "--release")
    set(cargo_profile_dir "release")
endif()

set(cargo_binary_dir "${CMAKE_CURRENT_SOURCE_DIR}/target/${cargo_profile_dir}")
set(generated_header "${CMAKE_CURRENT_SOURCE_DIR}/include/zenoh_flat.h")

if(APPLE)
    set(static_lib_name "libzenoh_flat_c.a")
    set(shared_lib_name "libzenoh_flat_c.dylib")
elseif(WIN32)
    set(static_lib_name "zenoh_flat_c.lib")
    set(shared_lib_name "zenoh_flat_c.dll")
else()
    set(static_lib_name "libzenoh_flat_c.a")
    set(shared_lib_name "libzenoh_flat_c.so")
endif()
set(static_lib "${cargo_binary_dir}/${static_lib_name}")
set(shared_lib "${cargo_binary_dir}/${shared_lib_name}")

# ── build the Rust sources via cargo ───────────────────────────────────────
file(GLOB_RECURSE rust_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.rs"
    "${CMAKE_CURRENT_SOURCE_DIR}/build.rs"
    "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
    "${CMAKE_CURRENT_SOURCE_DIR}/cbindgen.toml")

add_custom_command(
    OUTPUT "${static_lib}" "${shared_lib}" "${generated_header}"
    COMMAND ${CMAKE_COMMAND} -E echo "cargo build ${cargo_profile_flag}"
    COMMAND cargo build ${cargo_profile_flag}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS ${rust_sources}
    USES_TERMINAL
    VERBATIM)
add_custom_target(cargo ALL DEPENDS "${static_lib}" "${shared_lib}" "${generated_header}")

# ── imported library targets ───────────────────────────────────────────────
add_library(zenohflatc_shared SHARED IMPORTED GLOBAL)
add_dependencies(zenohflatc_shared cargo)
set_target_properties(zenohflatc_shared PROPERTIES IMPORTED_LOCATION "${shared_lib}")
target_include_directories(zenohflatc_shared INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(zenohflatc_static STATIC IMPORTED GLOBAL)
add_dependencies(zenohflatc_static cargo)
set_target_properties(zenohflatc_static PROPERTIES IMPORTED_LOCATION "${static_lib}")
target_include_directories(zenohflatc_static INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# Native libraries the Rust staticlib (zenoh + tokio) pulls in.
if(APPLE)
    target_link_libraries(zenohflatc_static INTERFACE
        pthread dl m
        "-framework Security" "-framework CoreFoundation" "-framework SystemConfiguration")
elseif(UNIX)
    target_link_libraries(zenohflatc_static INTERFACE pthread dl m rt)
endif()

if(BUILD_SHARED_LIBS)
    add_library(zenohflatc::lib ALIAS zenohflatc_shared)
else()
    add_library(zenohflatc::lib ALIAS zenohflatc_static)
endif()

# Expose the cargo binary dir so examples/tests can set their runtime RPATH.
set(ZENOHFLATC_CARGO_BINARY_DIR "${cargo_binary_dir}" CACHE INTERNAL "")

add_subdirectory(examples)
add_subdirectory(tests)
