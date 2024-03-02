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

#include <memory>

#include <tateyama/api/server/response.h>

namespace tateyama::endpoint::common {
/**
 * @brief response object for common_endpoint
 */
class response : public tateyama::api::server::response {
public:
    [[nodiscard]] bool check_cancel() const override {
        return cancel_;
    }

    void set_cancel() noexcept {
        cancel_ = true;
    }

private:
    bool cancel_{};
};

}  // tateyama::common::wire
