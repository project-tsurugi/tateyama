/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <glog/logging.h>

#include <takatori/util/detect.h>

#include <tateyama/utils/cache_align.h>
#include <tateyama/framework/component.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/service.h>

namespace tateyama::framework {

namespace details {

template <class T> using id_return_type = decltype(std::declval<T&>().id());

}

template<class T>
class cache_align repository {
public:
    /**
     * @brief iterate over added components
     * @param reverse_order indicates whether the iteration is done in reverse order
     */
    void each(std::function<void(T&)> consumer, bool reverse_order = false) {
        if(! reverse_order) {
            for(auto&& e : entity_) {
                consumer(*e);
            }
            return;
        }
        for(auto it = entity_.rbegin(), end = entity_.rend(); it != end; ++it) {
            consumer(**it);
        }
    }

    /**
     * @brief add new component
     */
    void add(std::shared_ptr<T> arg) {
        if constexpr (std::is_same_v<component::id_type, takatori::util::detect_t<details::id_return_type, T>>) {
            if (find_by_id(arg->id())) {
                LOG(ERROR) << "Components added with duplicate id.";
            }
        }
        entity_.emplace_back(std::move(arg));
    }

    /**
     * @brief find component by id
     * @return the found component
     * @return nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<T> find_by_id(component::id_type id) {
        for(auto&& x : entity_) {
            if(x->id() == id) {
                return x;
            }
        }
        return {};
    }

    /**
     * @brief find component by the tag for the given class
     * @return the found component
     * @return nullptr if not found
     */
    template<class U>
    std::shared_ptr<U> find() {
        static_assert(std::is_same_v<std::remove_const_t<decltype(U::tag)>, component::id_type>);
        auto c = find_by_id(U::tag);
        return std::static_pointer_cast<U>(c);
    }

    /**
     * @brief accessor to the number of elements
     * @return the number of elements added to this repository
     */
    [[nodiscard]] std::size_t size() const noexcept {
        return entity_.size();
    }

private:
    std::vector<std::shared_ptr<T>> entity_{};
};

}

