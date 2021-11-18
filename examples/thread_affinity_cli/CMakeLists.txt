file(GLOB SOURCES
        "*.cpp"
)

add_executable(thread-affinity-cli
        ${SOURCES}
)

set_target_properties(thread-affinity-cli
        PROPERTIES
        RUNTIME_OUTPUT_NAME "cli"
)

target_include_directories(thread-affinity-cli
        PRIVATE .
)

target_link_libraries(thread-affinity-cli
        PRIVATE tateyama-impl #FIXME
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE Threads::Threads
)

set_compile_options(thread-affinity-cli)

if(INSTALL_EXAMPLES)
    install_custom(thread-affinity-cli ${export_name})
endif()

add_test(
        NAME thread-affinity-cli
        COMMAND cli --minimum
)
