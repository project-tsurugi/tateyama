file(GLOB SOURCES
        "tateyama/*.cpp"
        "tateyama/task_scheduler/*.cpp"
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
add_library(engine
        ${SOURCES}
        ${GENERATED_PROTO_SRCS}
)

add_dependencies(engine
        build_protos
        )

set_target_properties(engine PROPERTIES
        OUTPUT_NAME ${export_name}
        )

target_include_directories(engine
        PRIVATE ${CMAKE_BINARY_DIR}/protos
        PRIVATE .
)

target_link_libraries(engine
        PUBLIC api
        PRIVATE takatori
        PRIVATE yugawara # temporary
        PRIVATE ${jogasaki_target} # temporary
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
)

if(MC_QUEUE)
    target_compile_definitions(engine PUBLIC MC_QUEUE)
endif()

# Boost.Thread doesn't seem to allow multiple versions to coexist.
# This version definition should be shared with caller at least.
target_compile_definitions(engine PUBLIC BOOST_THREAD_VERSION=4)

set_compile_options(engine)

install_custom(engine ${export_name})

# for tests
add_library(tateyama-impl INTERFACE)

target_include_directories(tateyama-impl
        INTERFACE .
        )

target_link_libraries(tateyama-impl
        INTERFACE engine
        INTERFACE ${jogasaki_target} # temporary
        INTERFACE takatori
        INTERFACE yugawara # temporary
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
