# Copyright 2018-2023 Project Tsurugi.
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

file(GLOB DATA_CHANNEL_WRITE_SOURCES
        "*.cpp"
        ${PROJECT_SOURCE_DIR}/test/tateyama/endpoint/ipc/ipc_client.cpp
        ${PROJECT_SOURCE_DIR}/test/tateyama/endpoint/ipc/ipc_test_utils.cpp
        ${PROJECT_SOURCE_DIR}/test/tateyama/endpoint/ipc/server_client_base.cpp
        )

add_executable(data_channel_write
        ${DATA_CHANNEL_WRITE_SOURCES}
        )

target_link_libraries(data_channel_write
        PRIVATE tateyama-impl
        PRIVATE tbb
        PRIVATE takatori
        PRIVATE atomic
        PRIVATE moodycamel
        PRIVATE rt
        PRIVATE limestone
        )

target_include_directories(data_channel_write
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}/bench/include
        PRIVATE ${CMAKE_BINARY_DIR}/src
        PRIVATE ${CMAKE_BINARY_DIR}/test
        PRIVATE ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${PROJECT_SOURCE_DIR}/src
        PRIVATE ${PROJECT_SOURCE_DIR}/test
        )
