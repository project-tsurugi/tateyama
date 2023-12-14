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

#include <string>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <tateyama/status/resource/core.h>

#include <tateyama/framework/component_ids.h>
#include <tateyama/framework/resource.h>
#include <tateyama/framework/environment.h>

#include <tateyama/status/resource/database_info_impl.h>

namespace tateyama::status_info::resource {

/**
 * @brief status resource bridge for tateyama framework
 * @details This object bridges status as a resource component in tateyama framework.
 * This object should be responsible only for life-cycle management.
 */
class bridge : public framework::resource {
    class shm_remover {
    public:
        explicit shm_remover(std::string name) : name_(std::move(name)) {
            boost::interprocess::shared_memory_object::remove(name_.c_str());
        }
        ~shm_remover(){
            boost::interprocess::shared_memory_object::remove(name_.c_str());
        }

        shm_remover(shm_remover const& other) = delete;
        shm_remover& operator=(shm_remover const& other) = delete;
        shm_remover(shm_remover&& other) noexcept = delete;
        shm_remover& operator=(shm_remover&& other) noexcept = delete;

    private:
        std::string name_;
    };

public:
    static constexpr std::size_t shm_size = 16384;

    static constexpr id_type tag = framework::resource_id_status;

    static constexpr std::string_view file_prefix = "tsurugidb-";  // NOLINT

    //@brief human readable label of this component
    static constexpr std::string_view component_label = "status_resource";

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

    /**
     * @brief set status in resource_status_memory
     */
    void whole(state s);

    /**
     * @brief wait for a shutdown request
     */
    void wait_for_shutdown();

    /**
     * @brief set proc_mutex file name
     */
    void mutex_file(std::string_view file_name);

    /**
     * @brief get proc_mutex file name
     */
    std::string_view mutex_file();

    /**
     * @brief remove session from ipc sessions
     */
    void set_maximum_sessions(std::size_t n);

    /**
     * @brief set database name of ipc sessions
     */
    void set_database_name(std::string_view name);

    /**
     * @brief add session to ipc sessions
     */
    void add_shm_entry(std::size_t session_id, std::size_t index);

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;

    /**
     * @brief returns a reference to the database_info
     */
    [[nodiscard]] const tateyama::api::server::database_info& database_info() const noexcept {
        return database_info_;
    }

private:
    bool deactivated_{false};

    std::unique_ptr<shm_remover> shm_remover_{};
    std::unique_ptr<boost::interprocess::managed_shared_memory> segment_;
    std::unique_ptr<resource_status_memory> resource_status_memory_{};
    std::string digest_{};
    database_info_impl database_info_{};

    void set_digest(const std::string& path_string);
};

} // namespace tateyama::status_info::resource
