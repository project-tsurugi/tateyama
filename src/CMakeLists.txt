if(NOT TARGET sharksfin-${SHARKSFIN_IMPLEMENTATION})
    message(FATAL_ERROR "sharksfin implementation \"sharksfin-${SHARKSFIN_IMPLEMENTATION}\" not found")
endif()

file(GLOB_RECURSE PROTO_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.proto")
add_custom_target(tateyama_protos DEPENDS ${PROTO_FILES})

# Compile protobuf and grpc files in tateyama_protos target to cpp
#
protobuf_generate(TARGET tateyama_protos LANGUAGE cpp OUT_VAR GENERATED_PROTO_FILES PROTOS ${PROTO_FILES})
if (ENABLE_GRPC)
  if(USE_GRPC_CONFIG)
      get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
  else ()
      find_program(grpc_cpp_plugin_location grpc_cpp_plugin REQUIRED)
  endif()
  protobuf_generate(TARGET tateyama_protos LANGUAGE grpc OUT_VAR GENERATED_GRPC_FILES PROTOS ${PROTO_FILES} GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")
else()
  set(GENERATED_GRPC_FILES)
endif()

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
        "tateyama/endpoint/stream/*.cpp"
        "tateyama/endpoint/stream/bootstrap/*.cpp"
        "tateyama/endpoint/loopback/*.cpp"
        "tateyama/endpoint/loopback/bootstrap/*.cpp"
        "tateyama/framework/*.cpp"
        "tateyama/loopback/*.cpp"
        "tateyama/server/*.cpp"
        "tateyama/server/impl/*.cpp"
        "tateyama/utils/*.cpp"
        "tateyama/configuration/*.cpp"
        "tateyama/session/*.cpp"
        "tateyama/session/service/*.cpp"
        "tateyama/session/resource/*.cpp"
        "tateyama/metrics/*.cpp"
        "tateyama/metrics/service/*.cpp"
        "tateyama/metrics/resource/*.cpp"
        "tateyama/request/service/*.cpp"
        "tateyama/authentication/resource/*.cpp"
        "tateyama/authentication/service/*.cpp"
        "tateyama/system/service/*.cpp"
        "tateyama/configuration/resource/*.cpp"
)
if (ENABLE_ALTIMETER)
    file(GLOB ALTIMETER_SOURCES
        "tateyama/altimeter/service/*.cpp"
    )
    list(APPEND SOURCES ${ALTIMETER_SOURCES})
endif()

if (ENABLE_GRPC)
    file(GLOB GRPC_SOURCES
        "tateyama/grpc/*.cpp"
        "tateyama/grpc/blob_relay/*.cpp"
    )
    list(APPEND SOURCES ${GRPC_SOURCES})
endif()

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
        ${GENERATED_PROTO_FILES}
        ${GENERATED_GRPC_FILES}
        PROPERTIES
        GENERATED TRUE
        COMPILE_FLAGS "-Wno-unused-parameter -Wno-array-bounds"
)

add_library(${ENGINE}
        ${SOURCES}
        ${GENERATED_PROTO_FILES}
        ${GENERATED_GRPC_FILES}
)

add_dependencies(${ENGINE}
        tateyama_protos
        )

set_target_properties(${ENGINE}
        PROPERTIES
                INSTALL_RPATH "\$ORIGIN"
                OUTPUT_NAME ${export_name}
)

target_include_directories(${ENGINE}
        PRIVATE
        PRIVATE ${CMAKE_BINARY_DIR}/src
        PRIVATE ${CMAKE_SOURCE_DIR}/third_party/cpp-httplib
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
        PRIVATE Boost::regex
        PRIVATE glog::glog
        PRIVATE atomic
        PRIVATE moodycamel
        PRIVATE protobuf::libprotobuf # temporary
        PRIVATE rt # for shm
        PRIVATE crypto
        PRIVATE jwt
)

if (ENABLE_GRPC)
target_link_libraries(${ENGINE}
        PRIVATE data-relay-grpc
        PRIVATE grpc++
        PRIVATE grpc
)
endif()

if(MC_QUEUE)
    target_compile_definitions(${ENGINE} PUBLIC MC_QUEUE)
endif()

if (ENABLE_DEBUG_SERVICE)
    target_compile_definitions(${ENGINE} PRIVATE ENABLE_DEBUG_SERVICE)
endif ()

if (ENABLE_ALTIMETER)
    target_link_libraries(${ENGINE}
        PRIVATE altimeter
    )
endif()

if (ENABLE_GRPC)
    target_compile_definitions(${ENGINE} PUBLIC ENABLE_GRPC)
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
