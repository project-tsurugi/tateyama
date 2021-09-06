file(GLOB SOURCES
        "tateyama/api/endpoint/mock/*.cpp"
)

add_library(tateyama-mock
        ${SOURCES}
)

target_include_directories(tateyama-mock
        PRIVATE .
)

# avoid name conflict with mock sharksfin implementation
set_target_properties(tateyama-mock PROPERTIES
        OUTPUT_NAME tateyama-mock-utils
        )

target_link_libraries(tateyama-mock
        PRIVATE tateyama-impl
        PRIVATE Boost::boost
        PRIVATE Boost::filesystem
        PRIVATE Boost::thread
        PRIVATE Boost::container
        PRIVATE glog::glog
)


# Boost.Thread doesn't seem to allow multiple versions to coexist.
# This version definition should be shared with caller at least.
target_compile_definitions(tateyama-mock PUBLIC BOOST_THREAD_VERSION=4)

set_compile_options(tateyama-mock)

# mock library needs install only when examples are used
if(INSTALL_EXAMPLES)
    install_custom(tateyama-mock ${export_name})
endif()

# for tests
add_library(tateyama-mock-impl INTERFACE)

target_include_directories(tateyama-mock-impl
        INTERFACE .
        )

target_link_libraries(tateyama-mock-impl
        INTERFACE tateyama
        INTERFACE takatori
        INTERFACE tbb
        INTERFACE numa
        INTERFACE tsl::hopscotch_map
        INTERFACE Boost::boost
        INTERFACE Boost::filesystem
        INTERFACE Boost::thread
        INTERFACE Boost::container
        INTERFACE glog::glog
        )
