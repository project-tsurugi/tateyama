set(test_target tateyama-test)

add_executable(${test_target}
        main.cpp
        )

target_include_directories(${test_target}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(${test_target}
        PRIVATE tateyama-impl
        PRIVATE mock-impl
        PUBLIC gtest
        PRIVATE tbb
        PRIVATE takatori
        PRIVATE atomic
        PRIVATE moodycamel
        )

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
        "tateyama/endpoint/*.cpp"
        "tateyama/task_scheduler/*.cpp"
        "tateyama/bootstrap/*.cpp"
        )

foreach(file ${SRCS})
    add_test_executable(${file})
endforeach()

