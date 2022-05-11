/*
 * Copyright 2018-2021 tsurugi project.
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

#include <tateyama/api/server/request.h>

#include <string_view>
#include <memory>

namespace tateyama::api::endpoint {
class request;
}

namespace tateyama::api::server::impl {

/**
 * @brief request interface
 */
class request : public server::request {
public:
    /**
     * @brief create empty object
     */
    request() = default;

    /**
     * @brief create new object
     */
    explicit request(std::shared_ptr<api::endpoint::request const> origin);

    request(request const& other) = default;
    request& operator=(request const& other) = default;
    request(request&& other) noexcept = default;
    request& operator=(request&& other) noexcept = default;

    /**
     * @brief destruct the object
     */
    ~request() override = default;

    /**
     * @brief accessor to session identifier
     */
    [[nodiscard]] std::size_t session_id() const override;

    /**
     * @brief accessor to target service identifier
     */
    [[nodiscard]] std::size_t service_id() const override;

    /**
     * @brief accessor to the payload binary data
     * @return the view to the payload
     */
    [[nodiscard]] std::string_view payload() const override;  //NOLINT(modernize-use-nodiscard)

    /**
     * @brief accessor to the endpoint request
     * @return the endpoint request
     */
    [[nodiscard]] std::shared_ptr<api::endpoint::request const> const& origin() const noexcept;

    /**
     * @brief create new object
     */
    friend std::shared_ptr<api::server::request> create_request(std::shared_ptr<api::endpoint::request const> origin);

private:
    std::shared_ptr<api::endpoint::request const> origin_{};
    std::size_t session_id_{};
    std::size_t service_id_{};
    std::string_view payload_{};

    bool init();
};

std::shared_ptr<api::server::request> create_request(std::shared_ptr<api::endpoint::request const> origin);

}
