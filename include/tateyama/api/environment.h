/*
 * Copyright 2018-2020 tsurugi project.
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

#include <takatori/util/sequence_view.h>

namespace tateyama::api {
namespace server {
class service;
}
namespace endpoint {
class provider;
class service;
}

}

namespace tateyama::api {

using takatori::util::sequence_view;

class environment {
public:
    // TODO add `get` and `remove` functions

    void add_application(std::shared_ptr<server::service> app);

    [[nodiscard]] sequence_view<std::shared_ptr<server::service> const> applications() const noexcept;

    void add_endpoint(std::shared_ptr<endpoint::provider> endpoint);

    [[nodiscard]] sequence_view<std::shared_ptr<endpoint::provider> const> endpoints() const noexcept;

    void endpoint_service(std::shared_ptr<endpoint::service> svc) noexcept;

    [[nodiscard]] std::shared_ptr<endpoint::service> const& endpoint_service() const noexcept;

private:
    std::vector<std::shared_ptr<server::service>> applications_{};
    std::vector<std::shared_ptr<endpoint::provider>> endpoints_{};
    std::shared_ptr<endpoint::service> service_{};
};

}