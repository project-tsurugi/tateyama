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

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/environment.h>

#include <sharksfin/api.h>
#include <limestone/status.h>
#include <limestone/api/datastore.h>

namespace tateyama::datastore::resource {

/**
 * @brief datastore resource bridge for tateyama framework
 * @details This object bridges datastore as a resource component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class bridge : public framework::resource {
public:
    static constexpr id_type tag = framework::resource_id_datastore;

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "datastore_resource";

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
    ~bridge() override;

    bridge(bridge const& other) = delete;
    bridge& operator=(bridge const& other) = delete;
    bridge(bridge&& other) noexcept = delete;
    bridge& operator=(bridge&& other) noexcept = delete;

    /**
     * @brief create empty object
     */
    bridge() = default;

    limestone::api::backup& begin_backup();
    std::unique_ptr<limestone::api::backup_detail> begin_backup(limestone::api::backup_type);
    void end_backup();
    limestone::status restore_backup(std::string_view, bool);
    limestone::status restore_backup(std::string_view, std::vector<limestone::api::file_set_entry>&);

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;

#if 0
    /**
     * @brief bridge to the limestone::api::datastore
     */
    std::vector<std::string> list_tags();
    void add_tag(std::string_view name, std::string_view comment);
    bool get_tag(std::string_view name, tag_info& out);
    bool remove_tag(std::string_view name);
#endif

private:
    limestone::api::datastore* datastore_{};
    bool deactivated_{false};
};

}
