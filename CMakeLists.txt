cmake_minimum_required(VERSION 3.24)
project(sikit_kicad LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- circuitcore: engines + widgets + parser, pinned ---
# For local hacking against a checkout instead of the tag:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_CIRCUITCORE=/path/to/circuitcore
include(FetchContent)
FetchContent_Declare(circuitcore
    GIT_REPOSITORY https://github.com/UnsignedChad/circuitcore.git
    GIT_TAG        v0.1.0-alpha.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(circuitcore)

# Qt is found inside the circuitcore subproject too, but imported
# targets are directory-scoped; our own target needs its own find.
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

# --- kicad ipc transport: nng + protobuf ---
find_package(nng CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# Generate C++ from the vendored kicad protos, preserving the directory
# layout so the generated cross-imports ("common/types/base_types.pb.h")
# resolve against the gen root.
set(KICAD_PROTOS
    board/board.proto
    board/board_commands.proto
    board/board_types.proto
    common/commands/base_commands.proto
    common/commands/editor_commands.proto
    common/commands/project_commands.proto
    common/envelope.proto
    common/types/base_types.proto
    common/types/enums.proto
    common/types/project_settings.proto
    schematic/schematic_commands.proto
    schematic/schematic_types.proto
)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto-gen)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_GEN_SRCS "")
foreach(rel ${KICAD_PROTOS})
    string(REPLACE ".proto" ".pb.cc" gen_src ${rel})
    string(REPLACE ".proto" ".pb.h"  gen_hdr ${rel})
    add_custom_command(
        OUTPUT  ${PROTO_GEN_DIR}/${gen_src} ${PROTO_GEN_DIR}/${gen_hdr}
        COMMAND protobuf::protoc
                -I ${CMAKE_CURRENT_SOURCE_DIR}/proto
                -I ${Protobuf_INCLUDE_DIRS}
                --cpp_out ${PROTO_GEN_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/proto/${rel}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/proto/${rel}
        COMMENT "protoc ${rel}"
        VERBATIM)
    list(APPEND PROTO_GEN_SRCS ${PROTO_GEN_DIR}/${gen_src})
endforeach()

add_library(kicad_api_proto STATIC ${PROTO_GEN_SRCS})
# SYSTEM: generated protobuf code is not ours to -Wall.
target_include_directories(kicad_api_proto SYSTEM PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(kicad_api_proto PUBLIC protobuf::libprotobuf)

# --- the plugin binary ---
add_executable(sikit-kicad
    src/main.cpp
    src/KiCadIpc.cpp
    # sikit's own top-level window, compiled from the pinned checkout.
    # It lives in the sikit exe target upstream, not in sikit::widgets,
    # so we pull the source in directly rather than duplicating it.
    ${circuitcore_SOURCE_DIR}/sikit/MainWindow.cpp
)
set_target_properties(sikit-kicad PROPERTIES AUTOMOC ON)
target_include_directories(sikit-kicad PRIVATE ${circuitcore_SOURCE_DIR}/sikit)
target_link_libraries(sikit-kicad PRIVATE
    sikit::widgets
    circuitcore::ui
    circuitcore::kicad
    kicad_api_proto
    nng::nng
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::OpenGL
)
target_compile_options(sikit-kicad PRIVATE -Wall -Wextra -Wpedantic)
