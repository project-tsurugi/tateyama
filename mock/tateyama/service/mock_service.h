/*
 * Copyright 2018-2025 Project Tsurugi.
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

#include <tateyama/framework/service.h>
#include <tateyama/framework/repository.h>
#include <tateyama/api/server/request.h>
#include <tateyama/api/server/response.h>
#include <tateyama/api/server/blob_info.h>
#include <tateyama/framework/environment.h>
#include <tateyama/framework/component_ids.h>

#include "mock_service.h"
#include "test_pattern.h"

namespace tateyama::service {

using tateyama::api::server::request;
using tateyama::api::server::response;

/**
* @brief mock_service for the mock_server
* @details This object provides mock service that mimics sql service
* This object should be responsible only for life-cycle management.
*/
class mock_service : public framework::service {
public:
    static constexpr id_type tag = framework::service_id_sql;

    [[nodiscard]] id_type id() const noexcept override;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "mock_service";

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

    bool operator()(
            std::shared_ptr<request> req,
            std::shared_ptr<response> res) override;

    /**
     * @brief create empty object
     */
    mock_service(bool write, bool clear) : pattern_(std::make_unique<test_pattern>(write, clear)) {
    }

    mock_service(mock_service const& other) = delete;
    mock_service& operator=(mock_service const& other) = delete;
    mock_service(mock_service&& other) noexcept = delete;
    mock_service& operator=(mock_service&& other) noexcept = delete;

    /**
     * @brief destructor the object
     */
    ~mock_service() override;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;

    void begin(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void prepare(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void execute_statement(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void execute_query(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void execute_prepared_statement(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void execute_prepared_query(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void commit(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void rollback(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void dispose_prepared_statement(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void explain(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void execute_dump(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void execute_load(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void describe_table(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void batch(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void list_tables(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void get_search_path(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void get_error_info(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void dispose_transaction(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void explain_by_text(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void extract_statement_info(std::shared_ptr<request> req, std::shared_ptr<response> res);
    void get_large_object_data(std::shared_ptr<request> req, std::shared_ptr<response> res);

private:
    std::unique_ptr<test_pattern> pattern_;
};

class blob_info_for_test : public tateyama::api::server::blob_info {
public:
    blob_info_for_test(std::string_view channel_name, std::filesystem::path path, bool temporary)
        : channel_name_(channel_name), path_(std::move(path)), temporary_(temporary) {
    }
    [[nodiscard]] std::string_view channel_name() const noexcept override {
        return channel_name_;
    }
    [[nodiscard]] std::filesystem::path path() const noexcept override {
        return path_;
    }
    [[nodiscard]] bool is_temporary() const noexcept override {
        return temporary_;
    }
    void dispose() override {
    }
private:
    const std::string channel_name_{};
    const std::filesystem::path path_{};
    const bool temporary_{};
};

} // namespace tateyama::mock
