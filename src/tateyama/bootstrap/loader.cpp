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
#include "loader.h"

#include <string_view>
#include <memory>
#include <dlfcn.h>
#include <glog/logging.h>

#include <takatori/util/fail.h>

#include <jogasaki/api.h>

#ifndef JOGASAKI_LIBRARY_NAME
#error missing jogasaki library name
#endif
// a trick to convert defined value to char literal
#define to_string_1(s) #s //NOLINT
#define to_string_2(s) to_string_1(s) //NOLINT
static constexpr auto jogasaki_library_name = to_string_2(JOGASAKI_LIBRARY_NAME); //NOLINT

namespace tateyama::bootstrap {

using takatori::util::fail;

namespace details {
loader::loader(std::string_view filename, int flags) :
    filename_(filename),
    flags_(flags),
    handle_(nullptr)
{
    open();
}

loader::~loader() {
    close();
}

void loader::open() {
    handle_ = dlopen(filename_.c_str(), flags_);
    if (nullptr == handle_) {
        throw_exception(dlerror());
    }
    dlerror(); // reset error
}

void loader::close() noexcept {
    if (nullptr != handle_) {
        dlclose(handle_);
        handle_ = nullptr;
    }
}

void* loader::lookup(std::string_view symbol) {
    void *fn = dlsym(handle_, symbol.data());
    if (nullptr == fn) {
        throw_exception(dlerror());
    }
    return fn;
}

void loader::throw_exception(char const* msg) {
    error_.assign(msg);
    throw std::runtime_error(error_);
}

loader& get_loader() {
    static loader ldr(jogasaki_library_name, RTLD_NOW);
    return ldr;
}

}

std::shared_ptr<jogasaki::api::database> create_database(jogasaki::configuration* cfg) {
    auto& ldr = details::get_loader();
    jogasaki::api::database* (*creater)(jogasaki::configuration*);
    void (*deleter)(jogasaki::api::database*);
    try {
        creater = reinterpret_cast<decltype(creater)>(ldr.lookup("new_database"));  //NOLINT
        deleter = reinterpret_cast<decltype(deleter)> (ldr.lookup("delete_database"));  //NOLINT
        return std::shared_ptr<jogasaki::api::database>{creater(cfg), [=](jogasaki::api::database* ptr) { deleter(ptr); }};
    } catch(const std::runtime_error& e) {
        LOG(ERROR) << e.what();
        fail();
    }
}

std::shared_ptr<jogasaki::api::environment> create_environment() {
    auto& ldr = details::get_loader();
    jogasaki::api::environment* (*creater)();
    void (*deleter)(jogasaki::api::environment*);
    try {
        creater = reinterpret_cast<decltype(creater)>(ldr.lookup("new_environment"));  //NOLINT
        deleter = reinterpret_cast<decltype(deleter)> (ldr.lookup("delete_environment"));  //NOLINT
        return std::shared_ptr<jogasaki::api::environment>{creater(), [=](jogasaki::api::environment* ptr) { deleter(ptr); }};
    } catch(const std::runtime_error& e) {
        LOG(ERROR) << e.what();
        fail();
    }
}

}