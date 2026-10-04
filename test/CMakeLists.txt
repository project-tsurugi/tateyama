set(test_target tateyama-test)

file(GLOB_RECURSE ProtoFiles RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.proto")
# By default, PROTOBUF_GENERATE_CPP generates file path for .pb.cc as if they are in the same directory.
# Work-around this with PROTOBUF_GENERATE_CPP_APPEND_PATH
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)
PROTOBUF_GENERATE_CPP(TEST_PROTO_SRCS TEST_PROTO_HDRS ${ProtoFiles})
add_custom_target(build_test_protos DEPENDS ${TEST_PROTO_SRCS})

set_source_files_properties(
        ${TEST_PROTO_SRCS}
        PROPERTIES
        GENERATED TRUE
        COMPILE_FLAGS -Wno-unused-parameter
)

add_executable(${test_target}
        main.cpp
        ${TEST_PROTO_SRCS}
        )

add_dependencies(${test_target}
        build_test_protos
        )

target_include_directories(${test_target}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_BINARY_DIR}/src
        PRIVATE ${CMAKE_BINARY_DIR}/test
        )

target_link_libraries(${test_target}
        PRIVATE gtest
        PRIVATE tateyama-impl
        PRIVATE tbb
        PRIVATE takatori
        PRIVATE limestone
        PRIVATE atomic
        PRIVATE moodycamel
        PRIVATE rt
        PRIVATE crypto
        PRIVATE jwt
        PRIVATE Boost::regex
        )

if (ENABLE_GRPC)
target_link_libraries(${test_target}
        PRIVATE data-relay-grpc
        PRIVATE ${TATEYAMA_GRPC_TARGET}
)
add_test(
        NAME distributed_query_role_test
        COMMAND ${test_target} --gtest_filter=distributed_query_role_test.* --gtest_output=xml:distributed_query_role_test_gtest_result.xml
)
# Validate the dependency mode selected by this build. The alternate mode may
# not be installed (for example, Ubuntu can provide only pkg-config metadata).
set(grpc_config_mode ${USE_GRPC_CONFIG})
add_test(
    NAME grpc_static_package_config_${grpc_config_mode}
    COMMAND ${CMAKE_COMMAND}
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/grpc-package-${grpc_config_mode}
        -DUSE_GRPC_CONFIG=${grpc_config_mode}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        "-DDEPENDENCY_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
        "-DDEPENDENCY_GRPC_DIR=${gRPC_DIR}"
        -DCONFIG_TEMPLATE=${CMAKE_SOURCE_DIR}/cmake/Config.cmake.in
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/grpc_package_config_test.cmake
)
endif()

function (add_test_executable source_file)
    get_filename_component(test_name "${source_file}" NAME_WE)
    target_sources(${test_target}
            PRIVATE ${source_file}
            )
    add_test(
            NAME ${test_name}
            COMMAND ${test_target} --gtest_filter=${test_name}.* --gtest_output=xml:${test_name}_gtest_result.xml
    )
endfunction (add_test_executable)

file(GLOB SRCS
        "tateyama/*.cpp"
        "tateyama/api/*.cpp"
        "tateyama/datastore/*.cpp"
        "tateyama/debug/*.cpp"
        "tateyama/endpoint/*.cpp"
        "tateyama/endpoint/ipc/*.cpp"
        "tateyama/endpoint/loopback/*.cpp"
        "tateyama/endpoint/stream/*.cpp"
        "tateyama/endpoint/common/*.cpp"
        "tateyama/task_scheduler/*.cpp"
        "tateyama/registry/*.cpp"
        "tateyama/common/*.cpp"
        "tateyama/framework/*.cpp"
        "tateyama/loopback/*.cpp"
        "tateyama/server/*.cpp"
        "tateyama/utils/*.cpp"
        "tateyama/session/*.cpp"
        "tateyama/metrics/*.cpp"
        "tateyama/metrics/resource/*.cpp"
        "tateyama/request/service/*.cpp"
        "tateyama/authentication/resource/*.cpp"
        "tateyama/system/*.cpp"
        "tateyama/configuration/*.cpp"
        )
if (ENABLE_ALTIMETER)
    file(GLOB ALTIMETER_SOURCES
        "tateyama/altimeter/*.cpp"
    )
    list(APPEND SRCS ${ALTIMETER_SOURCES})
endif()
if (ENABLE_GRPC)
    file(GLOB GRPC_SOURCES
        "tateyama/grpc/*.cpp"
        "tateyama/grpc/blob_relay/*.cpp"
        "tateyama/grpc/distributed_query/*.cpp"
    )
    list(APPEND SRCS ${GRPC_SOURCES})
endif()

list(REMOVE_ITEM SRCS ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/endpoint/ipc/ipc_resultset_writer_limit_test.cpp)

foreach(file ${SRCS})
    add_test_executable(${file})
endforeach()

file (GLOB RESULTSET_TRANSFER_TEST_SOURCES
  "tateyama/endpoint/ipc/ipc_resultset_writer_limit_test.cpp"
  "tateyama/endpoint/ipc/server_client_base.cpp"
  "tateyama/endpoint/ipc/ipc_test_utils.cpp"
  "tateyama/endpoint/ipc/ipc_client.cpp"
)

register_tests(
    TARGET tateyama
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/src
    DEPENDS
        tateyama-impl
        limestone
    SOURCES ${RESULTSET_TRANSFER_TEST_SOURCES}
    TEST_LABELS "RESULTSET_TRANSFER_IPC"
)
