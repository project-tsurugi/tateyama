/*
 * Copyright 2026-2026 Project Tsurugi.
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

#include "tateyama/status/resource/bridge.h"

#include <tateyama/configuration/configuration_provider.h>

namespace tateyama::configuration::resource {

class configuration_provider_impl : public tateyama::configuration::configuration_provider {
  public:
    [[nodiscard]] api::server::database_info const& database_info() const noexcept override;

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

    [[nodiscard]] id_type id() const noexcept override;

    /**
     * @see `tateyama::framework::component::label()`
     */
    [[nodiscard]] std::string_view label() const noexcept override;

  private:
    std::shared_ptr<status_info::resource::bridge> status_{};
};

}
