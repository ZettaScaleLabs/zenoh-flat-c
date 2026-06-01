#
# zenoh-flat-c tests — the supported subset ported (unmodified) from zenoh-c.
# Wired into ctest. Test type is derived from the filename prefix, like zenoh-c:
#   z_api_*  → unit   z_int_* → integration   z_leak_* → leak
#
message(STATUS "zenoh-flat-c tests")
add_custom_target(tests ALL)

find_package(Threads REQUIRED)

file(GLOB test_files "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
foreach(file ${test_files})
    get_filename_component(target ${file} NAME_WE)

    if(${target} MATCHES "^z_api_.*$")
        set(test_type "unit")
    elseif(${target} MATCHES "^z_int_.*$")
        set(test_type "integration")
    elseif(${target} MATCHES "^z_leak_.*$")
        set(test_type "leak")
    else()
        set(test_type "test")
    endif()

    add_executable(${target} ${file})
    add_dependencies(tests ${target})
    target_link_libraries(${target} PRIVATE zenohflatc::lib Threads::Threads)
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    set_target_properties(${target} PROPERTIES
        BUILD_RPATH "${ZENOHFLATC_CARGO_BINARY_DIR}")

    # Valgrind is unavailable on macOS; run leak tests directly (their main()
    # also asserts functional correctness, so a direct run is still meaningful).
    add_test(NAME "${test_type}_${target}" COMMAND ${target})
endforeach()
