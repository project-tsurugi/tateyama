/*
 * Copyright 2018-2022 tsurugi project.
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

#include <functional>
#include <memory>
#include <type_traits>

#include <tateyama/utils/cache_align.h>
#include <tateyama/framework/component.h>

namespace tateyama::framework {

template<class T>
class cache_align repository {
public:
    void each(std::function<void(T&)>);
    void add(std::shared_ptr<T>);
    T* find_by_id(component::id_type);

    //TODO use shared_ptr
    template<class U>
    U* find() {
        static_assert(std::is_same_v<decltype(U::tag), component::id_type>);
        auto* c = find_by_id(U::tag);
        return static_cast<U*>(c);
    }
};

}

