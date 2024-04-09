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

namespace tateyama::api::server {

/**
 * @brief an abstract super class of session data or their container.
 * @details clients can inherit this class and define extra properties needed for individual services.
 */
class session_element {
public:
    session_element() noexcept = default;
    session_element(session_element const&) = delete;
    session_element& operator=(session_element const&) = delete;
    session_element(session_element&&) noexcept = delete;
    session_element& operator=(session_element&&) noexcept = delete;
    virtual ~session_element() = default;

    /**
     * @brief disposes underlying resources of this element.
     * @throws std::runtime_error if failed to dispose this resource
     * @attention this function may not be called in some exceptional case,
     *    and the object will be destructed without calling this.
     *    In that case, the destructor must terminate rapidly without raising any exceptions.
     */
    virtual void dispose() = 0;
};

}
