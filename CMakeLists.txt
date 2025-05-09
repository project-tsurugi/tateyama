# Copyright 2018-2024 Project Tsurugi.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.16)

project(tateyama
    VERSION 1.0.0
    DESCRIPTION "Tateyama Application Infrastructure"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(ENABLE_SANITIZER "enable sanitizer on debug build" ON)
option(ENABLE_UB_SANITIZER "enable undefined behavior sanitizer on debug build" OFF)
option(ENABLE_COVERAGE "enable coverage on debug build" OFF)
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_BENCHMARK "Build benchmark programs" ON)
option(BUILD_DOCUMENTS "build documents" ON)
option(BUILD_MOCK "Build mock server" OFF)
option(BUILD_STRICT "build with option strictly determine of success" ON)
option(INSTALL_EXAMPLES "install examples" OFF)
option(BUILD_SHARED_LIBS "build shared libraries instead of static" ON)
option(ENABLE_CACHE_ALIGN "enable optional cache align requirement" OFF)
option(MC_QUEUE "use moody-camel queue" OFF)
option(TRACY_ENABLE "enable tracy profiler" OFF)
option(ENABLE_DEBUG_SERVICE "treats debug service as a core component" ON)
option(ENABLE_ALTIMETER "enable altimeter logging" OFF)

if(NOT DEFINED SHARKSFIN_IMPLEMENTATION)
    set(
            SHARKSFIN_IMPLEMENTATION "shirakami"
            CACHE STRING
            "sharksfin target name to link"
            FORCE
    )
endif()
set(ENGINE "engine")

find_package(takatori REQUIRED)
find_package(sharksfin REQUIRED)
find_package(limestone REQUIRED)
find_package(moodycamel REQUIRED)
find_package(Doxygen)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost
        COMPONENTS filesystem thread system container
        REQUIRED
        )
find_package(TBB REQUIRED)
find_package(Protobuf REQUIRED)
find_package(mpdecpp REQUIRED)
if (ENABLE_ALTIMETER)
    find_package(altimeter REQUIRED)
    find_package(fmt REQUIRED)
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)
include(InstallOptions)
include(Tests)

add_subdirectory(third_party) # should be before enable_testing()

if (BUILD_TESTS OR BUILD_EXAMPLES)
    enable_testing()
endif()

set(export_name "tateyama-${SHARKSFIN_IMPLEMENTATION}")
set(package_name "tateyama-${SHARKSFIN_IMPLEMENTATION}")

add_library(api INTERFACE)

target_include_directories(api
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/tateyama>
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${package_name}-config.cmake
    @ONLY
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${package_name}-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)

install_custom(api ${export_name})

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${package_name}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${package_name}-config-version.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${package_name}
)

install(
    EXPORT ${package_name}
    NAMESPACE ${package_name}-
    FILE ${package_name}-targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${package_name}
    EXPORT_LINK_INTERFACE_LIBRARIES
)

add_subdirectory(src)
if(BUILD_TESTS)
    add_subdirectory(test)
endif()
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if (BUILD_BENCHMARK)
    add_subdirectory(bench)
endif ()
if (BUILD_DOCUMENTS)
    add_subdirectory(doxygen)
endif()
if(BUILD_MOCK)
    add_subdirectory(mock)
endif()

