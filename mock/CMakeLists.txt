file(GLOB SOURCES
        "tateyama/api/endpoint/mock/*.cpp"
        "tateyama/utils/*.cpp"
)

set(mock "mock")

add_library(mock-api INTERFACE)

target_link_libraries(mock-api
        INTERFACE ${jogasaki_api}
        )

target_include_directories(mock-api
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/tateyama>
        )

install_custom(mock-api ${export_name})

add_library(${mock}
        ${SOURCES}
)

target_include_directories(${mock}
        PRIVATE include
)

# avoid name conflict with mock sharksfin implementation
set_target_properties(${mock} PROPERTIES
        OUTPUT_NAME tateyama-${SHARKSFIN_IMPLEMENTATION}-mock
        )

target_link_libraries(${mock}
        PUBLIC mock-api
        PRIVATE tateyama-impl
        PRIVATE Boost::boost
        PRIVATE Boost::filesystem
        PRIVATE Boost::thread
        PRIVATE Boost::container
        PRIVATE glog::glog
)


# Boost.Thread doesn't seem to allow multiple versions to coexist.
# This version definition should be shared with caller at least.
target_compile_definitions(${mock} PUBLIC BOOST_THREAD_VERSION=4)

set_compile_options(${mock})

# mock library needs install only when examples are used
if(INSTALL_MOCK)
    install_custom(${mock} ${export_name})
endif()

# for tests
add_library(mock-impl INTERFACE)

target_include_directories(mock-impl
        INTERFACE include
        )

target_link_libraries(mock-impl
        INTERFACE ${ENGINE}
        INTERFACE ${jogasaki_api}
        INTERFACE takatori
        INTERFACE yugawara
        INTERFACE tbb
        INTERFACE numa
        INTERFACE tsl::hopscotch_map
        INTERFACE Boost::boost
        INTERFACE Boost::filesystem
        INTERFACE Boost::thread
        INTERFACE Boost::container
        INTERFACE glog::glog
        INTERFACE protobuf::libprotobuf # temporary
        )
