# Copyright 2018-2023 Tsurugi project.
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

if(TARGET jemalloc::jemalloc)
    return()
endif()

find_library(jemalloc_LIBRARY_FILE NAMES jemalloc)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(jemalloc DEFAULT_MSG
    jemalloc_LIBRARY_FILE)

if(jemalloc_LIBRARY_FILE)
    set(jemalloc_FOUND ON)
    add_library(jemalloc::jemalloc SHARED IMPORTED)
    set_target_properties(jemalloc::jemalloc PROPERTIES
        IMPORTED_LOCATION "${jemalloc_LIBRARY_FILE}")
else()
    set(jemalloc_FOUND OFF)
endif()

unset(jemalloc_LIBRARY_FILE CACHE)
