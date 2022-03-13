
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT wayland-client>=1.13.0 REQUIRED)

find_program(WAYLAND_SCANNER_EXE NAMES wayland-scanner)

function(wayland_generate_proto PATH)
    get_filename_component(FILENAME ${PATH} NAME)
    string(REGEX REPLACE "\\.[^.]*$" "" FILENAME_NOEXT ${FILENAME})

    add_custom_command(
        OUTPUT  ${FILENAME_NOEXT}-client-protocol.h
        COMMAND ${WAYLAND_SCANNER_EXE} client-header
            < ${PATH}
            > ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME_NOEXT}-client-protocol.h
        DEPENDS ${PATH}
        COMMENT "Generating ${FILENAME_NOEXT} protocol header"
    )
    add_custom_command(
        OUTPUT  ${FILENAME_NOEXT}-protocol.c
        COMMAND ${WAYLAND_SCANNER_EXE} private-code
            < ${PATH}
            > ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME_NOEXT}-protocol.c
        DEPENDS ${PATH}
        COMMENT "Generating ${FILENAME_NOEXT} protocol source code"
    )
endfunction()

function(wayland_proto_library NAME)
    set(proto_headers "")
    set(proto_sources "")
    foreach(proto IN ITEMS ${ARGN})
        list(APPEND proto_headers ${CMAKE_CURRENT_BINARY_DIR}/${proto}-client-protocol.h)
        list(APPEND proto_sources ${CMAKE_CURRENT_BINARY_DIR}/${proto}-protocol.c)
    endforeach()

    add_library(${NAME} STATIC ${proto_headers} ${proto_sources})
    target_include_directories(${NAME} PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${WAYLAND_CLIENT_INCLUDE_DIRS}
    )
    target_link_libraries(${NAME} PUBLIC
        ${WAYLAND_CLIENT_LIBRARIES}
    )
endfunction()
