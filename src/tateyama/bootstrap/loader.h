/*
 * Copyright 2018-2020 tsurugi project.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <string_view>
#include <atomic>
#include <memory>

#include <jogasaki/api.h>
#include <tateyama/status.h>

#include <cstring>

namespace tateyama::bootstrap {

namespace details {

/**
 * @brief dynamic library loader
 * @details
 */
class loader {
public:
    explicit loader(std::string_view filename, int flags);
    loader(loader const& src) = delete;
    loader(loader&&) = delete;
    ~loader();

    loader& operator = (loader const&) = delete;
    loader& operator = (loader&&) = delete;

    void* lookup(std::string_view symbol);

private:
    std::string filename_;
    int flags_;
    void* handle_;
    std::string error_{};

    void open();
    void close() noexcept;
    void throw_exception(const char* msg);
};

}

/**
 * @brief load and create environment
 * @details load necessary SQL engine libraries and create environment.
 * This will initialize the environment for SQL engine. Call this first before start using any other jogasaki functions.
 * @note TODO revisit when components boundary is updated
 * @warning ASAN and dlopen has compatibility issue (https://bugs.llvm.org/show_bug.cgi?id=27790).
 * Specify install prefix to LD_LIBRARY_PATH when ASAN is used (e.g. on Debug build).
 */
std::shared_ptr<jogasaki::api::environment> create_environment();

/**
 * @brief load and create database
 * @details load necessary SQL engine libraries and create database.
 * @param cfg the configuration used to create the database
 * @return the database object
 * @note TODO revisit when components boundary is updated
 * @warning ASAN and dlopen has compatibility issue (https://bugs.llvm.org/show_bug.cgi?id=27790).
 * Specify install prefix to LD_LIBRARY_PATH when ASAN is used (e.g. on Debug build).
 */
std::shared_ptr<jogasaki::api::database> create_database(jogasaki::configuration* cfg);

}










