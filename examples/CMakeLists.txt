#
# zenoh-flat-c examples — ported (unmodified) from zenoh-c's closure-core set.
# Each builds against the zenoh-c-compatible API (`zenoh.h`).
#
message(STATUS "zenoh-flat-c examples")
add_custom_target(examples ALL)

file(GLOB example_files "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
foreach(file ${example_files})
    get_filename_component(target ${file} NAME_WE)
    add_executable(${target} ${file})
    add_dependencies(examples ${target})
    target_link_libraries(${target} PRIVATE zenohflatc::lib)
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    # Resolve the cargo-built dylib at runtime.
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${ZENOHFLATC_CARGO_BINARY_DIR}")
endforeach()
