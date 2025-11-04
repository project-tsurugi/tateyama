/*
 * Copyright 2025-2025 Project Tsurugi.
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

#include <cstdint>
#include <optional>
#include <memory>

#include <tateyama/framework/resource.h>
#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/environment.h>

#include <tateyama/grpc/blob_session.h>

namespace tateyama::grpc {

class resource_impl;

class blob_relay_service_resource : public ::tateyama::framework::resource {
public:
    // resource fundamentals
    static constexpr id_type tag = framework::resource_id_blob_relay_service;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "blob_relay_service_resource";

    /// @brief the transaction ID type.
    using transaction_id_type = std::uint64_t;

    /**
      * @brief Create a new session for BLOB operations.
      * @param transaction_id The ID of the transaction that owns the session,
      *    or empty if the session is not associated with any transaction
      * @return the created session object
      */
    [[nodiscard]] std::shared_ptr<blob_session> create_session(std::optional<transaction_id_type> transaction_id = {});


    // resource fundamentals
    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @brief setup the component (the state will be `ready`)
     */
    bool setup(framework::environment&) override;

    /**
     * @brief start the component (the state will be `activated`)
     */
    bool start(framework::environment&) override;

    /**
     * @brief shutdown the component (the state will be `deactivated`)
     */
    bool shutdown(framework::environment&) override;

    /**
     * @brief destructor the object
     */
    ~blob_relay_service_resource() override;

    blob_relay_service_resource(blob_relay_service_resource const& other) = delete;
    blob_relay_service_resource& operator=(blob_relay_service_resource const& other) = delete;
    blob_relay_service_resource(blob_relay_service_resource&& other) noexcept = delete;
    blob_relay_service_resource& operator=(blob_relay_service_resource&& other) noexcept = delete;

    /**
     * @brief create empty object
     */
    blob_relay_service_resource();

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;


private:
    std::unique_ptr<resource_impl, void(*)(resource_impl*)> impl_;
};

} // namespace
