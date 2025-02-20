if(NOT TARGET sharksfin-${SHARKSFIN_IMPLEMENTATION})
    message(FATAL_ERROR "sharksfin implementation \"sharksfin-${SHARKSFIN_IMPLEMENTATION}\" not found")
endif()

file(GLOB_RECURSE ProtoFiles RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.proto")

# By default, PROTOBUF_GENERATE_CPP generates file path for .pb.cc as if they are in the same directory.
# Work-around this with PROTOBUF_GENERATE_CPP_APPEND_PATH
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)
PROTOBUF_GENERATE_CPP(GENERATED_PROTO_SRCS GENERATED_PROTO_HDRS ${ProtoFiles})
add_custom_target(build_protos_for_mock DEPENDS ${GENERATED_PROTO_SRCS})

file(GLOB SOURCES
        "tateyama/server/*.cpp"
        "tateyama/service/*.cpp"
        "tateyama/faulty_service/*.cpp"
        "jogasaki/api/impl/*.cpp"
        "jogasaki/serializer/*.cpp"
)

set_source_files_properties(
        ${GENERATED_SQL_PROTO_SRCS}
        PROPERTIES
        GENERATED TRUE
        COMPILE_FLAGS "-Wno-unused-parameter -Wno-array-bounds"
)

add_executable(mock_server
        ${SOURCES}
        ${GENERATED_PROTO_SRCS}
)

add_dependencies(mock_server
        build_protos_for_mock
        )

set_target_properties(mock_server
        PROPERTIES
                INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
                RUNTIME_OUTPUT_NAME "mock_server"
        )

target_include_directories(mock_server
        PRIVATE ${CMAKE_BINARY_DIR}/src
        PRIVATE ${CMAKE_BINARY_DIR}/mock
        PRIVATE ${CMAKE_SOURCE_DIR}/src
        PRIVATE .
)

target_link_libraries(mock_server
        PUBLIC api
        PRIVATE tateyama-impl
        PRIVATE takatori
        PRIVATE Boost::boost
        PRIVATE Boost::filesystem
        PRIVATE Boost::thread
        PRIVATE Boost::container
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE protobuf::libprotobuf
        )

# Boost.Thread doesn't seem to allow multiple versions to coexist.
# This version definition should be shared with caller at least.
target_compile_definitions(mock_server PUBLIC BOOST_THREAD_VERSION=4)

set_compile_options(mock_server)

# target_name = mock_server, export_name = mock_server
install_custom(mock_server mock_server)
