if(NOT TARGET sharksfin-${SHARKSFIN_IMPLEMENTATION})
    message(FATAL_ERROR "sharksfin implementation \"sharksfin-${SHARKSFIN_IMPLEMENTATION}\" not found")
endif()

file(GLOB_RECURSE ProtoFiles RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.proto")

# By default, PROTOBUF_GENERATE_CPP generates file path for .pb.cc as if they are in the same directory.
# Work-around this with PROTOBUF_GENERATE_CPP_APPEND_PATH
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)
PROTOBUF_GENERATE_CPP(GENERATED_PROTO_SRCS GENERATED_PROTO_HDRS ${ProtoFiles})
add_custom_target(build_protos DEPENDS ${GENERATED_PROTO_SRCS})

install(DIRECTORY
        ${CMAKE_BINARY_DIR}/src/tateyama
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tateyama
        FILES_MATCHING
        PATTERN "*.pb.h"
)

file(GLOB SOURCES
        "tateyama/*.cpp"
        "tateyama/datastore/*.cpp"
        "tateyama/datastore/resource/*.cpp"
        "tateyama/datastore/service/*.cpp"
        "tateyama/debug/*.cpp"
        "tateyama/status/resource/*.cpp"
        "tateyama/diagnostic/resource/*.cpp"
        "tateyama/endpoint/service/*.cpp"
        "tateyama/endpoint/service/impl/*.cpp"
        "tateyama/endpoint/ipc/*.cpp"
        "tateyama/endpoint/ipc/bootstrap/*.cpp"
        "tateyama/endpoint/loopback/*.cpp"
        "tateyama/endpoint/loopback/bootstrap/*.cpp"
        "tateyama/endpoint/stream/*.cpp"
        "tateyama/endpoint/stream/bootstrap/*.cpp"
        "tateyama/framework/*.cpp"
        "tateyama/loopback/*.cpp"
        "tateyama/server/*.cpp"
        "tateyama/server/impl/*.cpp"
        "tateyama/utils/*.cpp"
        "tateyama/configuration/*.cpp"
)

if(TRACY_ENABLE)
    file(GLOB TRACY_CLIENT
            "../third_party/tracy/public/TracyClient.cpp"
            )
    set_source_files_properties(
        ${TRACY_CLIENT}
        PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers -Wno-empty-body"
    )
    list(APPEND SOURCES ${TRACY_CLIENT})
endif()

set_source_files_properties(
        ${GENERATED_PROTO_SRCS}
        PROPERTIES
        GENERATED TRUE
        COMPILE_FLAGS "-Wno-unused-parameter -Wno-array-bounds"
)

add_library(${ENGINE}
        ${SOURCES}
        ${GENERATED_PROTO_SRCS}
)

add_dependencies(${ENGINE}
        build_protos
        )

set_target_properties(${ENGINE} PROPERTIES
        OUTPUT_NAME ${export_name}
        )

target_include_directories(${ENGINE}
        PRIVATE
        PRIVATE ${CMAKE_BINARY_DIR}/src
        PRIVATE .
)

target_link_libraries(${ENGINE}
        PUBLIC api
        PUBLIC sharksfin-${SHARKSFIN_IMPLEMENTATION}
        PRIVATE takatori
        PRIVATE limestone
        PRIVATE tbb
        PRIVATE numa
        PRIVATE Boost::boost
        PRIVATE Boost::filesystem
        PRIVATE Boost::thread
        PRIVATE Boost::container
        PRIVATE glog::glog
        PRIVATE atomic
        PRIVATE moodycamel
        PRIVATE protobuf::libprotobuf # temporary
        PRIVATE rt # for shm
        PRIVATE crypto
)

if(MC_QUEUE)
    target_compile_definitions(${ENGINE} PUBLIC MC_QUEUE)
endif()

if (ENABLE_ALTIMETER)
    target_link_libraries(${ENGINE}
        PRIVATE altimeter
    )
    target_compile_definitions(${ENGINE} PUBLIC ALTIMETER)
endif()

# Boost.Thread doesn't seem to allow multiple versions to coexist.
# This version definition should be shared with caller at least.
target_compile_definitions(${ENGINE} PUBLIC BOOST_THREAD_VERSION=4)

set_compile_options(${ENGINE})

install_custom(${ENGINE} ${export_name})

# for tests
add_library(tateyama-impl INTERFACE)

target_include_directories(tateyama-impl
        INTERFACE .
        )

target_link_libraries(tateyama-impl
        INTERFACE ${ENGINE}
        INTERFACE takatori
        INTERFACE tbb
        INTERFACE numa
        INTERFACE moodycamel
        INTERFACE Boost::boost
        INTERFACE Boost::filesystem
        INTERFACE Boost::thread
        INTERFACE Boost::container
        INTERFACE glog::glog
        INTERFACE protobuf::libprotobuf # temporary
        )

if(MC_QUEUE)
    target_compile_definitions(tateyama-impl INTERFACE MC_QUEUE)
endif()

