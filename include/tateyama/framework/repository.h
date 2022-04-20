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
    /**
     * @brief iterate over added components
     */
    void each(std::function<void(T&)>);

    /**
     * @brief add new component
     */
    void add(std::shared_ptr<T>);

    /**
     * @brief find component by id
     * @return the found component
     * @return nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<T> find_by_id(component::id_type);

    /**
     * @brief find component by the tag for the given class
     * @return the found component
     * @return nullptr if not found
     */
    template<class U>
    std::shared_ptr<U> find() {
        static_assert(std::is_same_v<decltype(U::tag), component::id_type>);
        auto* c = find_by_id(U::tag);
        return static_cast<U*>(c);
    }
};

}

