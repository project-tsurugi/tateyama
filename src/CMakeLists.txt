file(GLOB SOURCES
        "tateyama/*.cpp"
        "tateyama/bootstrap/*.cpp"
        "tateyama/server/*.cpp"
        "tateyama/task_scheduler/*.cpp"
        "tateyama/registry/*.cpp"
        "tateyama/endpoint/service/*.cpp"
        "tateyama/endpoint/service/impl/*.cpp"
        "tateyama/endpoint/ipc/*.cpp"
        "tateyama/endpoint/ipc/bootstrap/*.cpp"
        "tateyama/endpoint/stream/*.cpp"
        "tateyama/endpoint/stream/bootstrap/*.cpp"
        "tateyama/framework/*.cpp"
        "tateyama/utils/*.cpp"
)

if(TRACY_ENABLE)
    file(GLOB TRACY_CLIENT
            "../third_party/tracy/TracyClient.cpp"
            )
    list(APPEND SOURCES ${TRACY_CLIENT})
endif()

set_source_files_properties(
        ${GENERATED_PROTO_SRCS}
        PROPERTIES
        GENERATED TRUE
        COMPILE_FLAGS -Wno-unused-parameter
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
        PRIVATE ${CMAKE_BINARY_DIR}/protos
        PRIVATE .
)

target_link_libraries(${ENGINE}
        PUBLIC api
        PRIVATE takatori
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
)

if(MC_QUEUE)
    target_compile_definitions(${ENGINE} PUBLIC MC_QUEUE)
endif()

# Boost.Thread doesn't seem to allow multiple versions to coexist.
# This version definition should be shared with caller at least.
target_compile_definitions(${ENGINE} PUBLIC BOOST_THREAD_VERSION=4)

target_compile_definitions(${ENGINE} PUBLIC JOGASAKI_LIBRARY_NAME=${jogasaki_library_name})

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

target_compile_definitions(tateyama-impl INTERFACE JOGASAKI_LIBRARY_NAME=${jogasaki_library_name})

