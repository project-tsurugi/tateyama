/*
 * Copyright 2018-2024 Project Tsurugi.
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

#include <map>
#include <boost/thread/shared_mutex.hpp>

#include <tateyama/api/server/session_element.h>

namespace tateyama::api::server {

/**
 * @brief a storage for each service to keep session-specific data.
 * @attention this storage is NOT thread-safe, clients should use this only on the request threads.
 */
class session_store {
public:
    /// @brief the element ID type
    using id_type = std::size_t;

    /**
     * @brief registers a new element as a session data.
     * @tparam T the element type
     * @param element_id the element ID to register
     * @param element the element to register
     * @returns true if successfully registered the element
     * @returns false if another element already exists in this store
     */
    template<class T> // static_assert(std::is_base_of_v<session_element, T>)
    bool put(id_type element_id, std::shared_ptr<T> element) {
        static_assert(std::is_base_of<session_element, T>::value, "T is not a derived class of session_element");  // NOLINT(modernize-type-traits)
        boost::unique_lock<boost::upgrade_mutex> lock(mtx_);

        if (auto itr = element_map_.find(element_id); itr != element_map_.end()) {
            return false;
        }
        element_map_.emplace(element_id, element);
        return true;
    }

    /**
     * @brief obtains the stored session data.
     * @param element_id the ID of the stored data
     * @tparam T the element type
     * @returns the stored session data if it exists
     * @returns empty std::shared_ptr if it is absent, or element type was mismatched
     */
    template<class T> // static_assert(std::is_base_of_v<session_element, T>)
    [[nodiscard]] std::shared_ptr<T> find(id_type element_id) const {
        static_assert(std::is_base_of<session_element, T>::value, "T is not a derived class of session_element");  // NOLINT(modernize-type-traits)
        boost::shared_lock<boost::upgrade_mutex> lock(mtx_);

        if (auto itr = element_map_.find(element_id); itr != element_map_.end()) {
            return std::dynamic_pointer_cast<T>(itr->second);
        }
        return {};
    }

    /**
     * @brief obtains the stored session data, or creates new session data and register.
     * @details if such the session data is absent, this will creates a new element data
     *     by calling `std::make_shared<T>(std::forward<Args>(args)...)`.
     * @tparam T the element type
     * @tparam Args the element constructor parameter types
     * @param element_id the target element ID
     * @param args the constructor arguments for emplacing a new element
     * @returns the stored session data if it exists
     * @returns the emplaced session data if it does not exist
     * @returns empty if the existing element type was mismatched
     */
    template<class T, class... Args> // static_assert(std::is_base_of_v<session_element, T>)
    [[nodiscard]] std::shared_ptr<T> find_or_emplace(id_type element_id, Args&&...args) {
        static_assert(std::is_base_of<session_element, T>::value, "T is not a derived class of session_element");  // NOLINT(modernize-type-traits)
        boost::upgrade_lock<boost::upgrade_mutex> ug_lock(mtx_);
        
        if (auto itr = element_map_.find(element_id); itr != element_map_.end()) {
            return std::dynamic_pointer_cast<T>(itr->second);
        }
        {
            boost::upgrade_to_unique_lock<boost::upgrade_mutex> ex_lock(ug_lock);
            element_map_.emplace(element_id, std::make_shared<T>(std::forward<Args>(args)...));
        }
        return {};
    }

    /**
     * @brief removes the stored session data.
     * @param element_id the ID of the stored data
     * @return true if successfully removed the stored data, or it is already absent
     * @return false if the element type was mismatched
     * @attention this never call session_element::dispose() for the removed.
     */
    template<class T> // static_assert(std::is_base_of_v<session_element, T>)
    bool remove(id_type element_id) {
        static_assert(std::is_base_of<session_element, T>::value, "T is not a derived class of session_element");  // NOLINT(modernize-type-traits)
        boost::unique_lock<boost::upgrade_mutex> lock(mtx_);

        if (auto itr = element_map_.find(element_id); itr != element_map_.end()) {
            if (std::dynamic_pointer_cast<T>(itr->second)) {
                element_map_.erase(itr);
                return true;
            }
            return false;
        }
        return true;
    }

    /**
     * @brief disposes underlying resources of this session store.
     * @throws std::runtime_error if failed to dispose this session store
     */
    void dispose() {
        boost::unique_lock<boost::upgrade_mutex> lock(mtx_);

        for (auto&& e: element_map_) {
            e.second->dispose();
        }
    }

private:
    std::map<id_type, std::shared_ptr<session_element>> element_map_{};
    mutable boost::upgrade_mutex mtx_{};
};

}
