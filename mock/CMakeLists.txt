add_library(mock INTERFACE)

target_include_directories(mock
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/tateyama>
        )

install_custom(mock ${export_name})
