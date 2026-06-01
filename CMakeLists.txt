#
# CMake build for zenoh-flat-c.
#
# Modeled on eclipse-zenoh/zenoh-c's CMakeLists.txt: a hand-rolled
# `add_custom_command` invokes `cargo build` to produce the Rust library and the
# generated C header, then IMPORTED targets expose it as `zenohflatc::lib`.
#
# The crate's sole C API is the generated `include/zenoh_flat.h` (plain `z_*`
# symbols) + `include/zenoh_flat_configure.h` (feature `#define`s, auto-included
# by `zenoh_flat.h`).
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

# Feature toggles (mirror zenoh-c's ZENOHC_BUILD_WITH_*; default OFF). When set,
# the corresponding Cargo feature is passed to `cargo build`, which forwards it
# to zenoh-flat → zenoh. `unstable` adds the `reliability` QoS surface to the C
# ABI (gated by `Z_FEATURE_UNSTABLE_API` in the headers).
set(ZENOHFLATC_BUILD_WITH_UNSTABLE_API OFF CACHE BOOL "Enable the unstable API feature")
set(ZENOHFLATC_BUILD_WITH_SHARED_MEMORY OFF CACHE BOOL "Enable the shared-memory feature")
set(cargo_feature_flags "")
if(ZENOHFLATC_BUILD_WITH_UNSTABLE_API)
    list(APPEND cargo_feature_flags "--features=unstable")
endif()
if(ZENOHFLATC_BUILD_WITH_SHARED_MEMORY)
    list(APPEND cargo_feature_flags "--features=shared-memory")
endif()

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

# Always invoke cargo (it is the real incremental engine, and — unlike a
# CMake OUTPUT rule — it correctly rebuilds when the feature set changes, since
# the static/shared lib filenames don't encode the features). The DEPENDS list
# is kept for IDE source grouping.
add_custom_target(cargo ALL
    COMMAND ${CMAKE_COMMAND} -E echo "cargo build ${cargo_profile_flag} ${cargo_feature_flags}"
    COMMAND cargo build ${cargo_profile_flag} ${cargo_feature_flags}
    BYPRODUCTS "${static_lib}" "${shared_lib}" "${generated_header}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCES ${rust_sources}
    USES_TERMINAL
    VERBATIM)

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
