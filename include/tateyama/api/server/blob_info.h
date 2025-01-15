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

#include <string_view>
#include <filesystem>
#include <cstddef>
#include <tateyama/status.h>

namespace tateyama::api::server {

/**
 * @brief an abstract super class of BLOB data from the client.
 */
class blob_info {
public:
    /**
     * @brief create empty object
     */
    blob_info() = default;

    /**
     * @brief destruct the object
     */
    virtual ~blob_info() = default;

    blob_info(blob_info const& other) = default;
    blob_info& operator=(blob_info const& other) = default;
    blob_info(blob_info&& other) noexcept = default;
    blob_info& operator=(blob_info&& other) noexcept = default;

    /**
     * @brief returns the channel name of the BLOB data.
     * @return the channel name
     */
    [[nodiscard]] virtual std::string_view channel_name() const noexcept = 0;

    /**
     * @brief returns the path of the file that represents the BLOB.
     * @details If the BLOB data is temporary, the file may has been lost.
     * @return the path of the file
     * @see is_temporary()
     */
    [[nodiscard]] virtual std::filesystem::path path() const noexcept = 0;

    /**
     * @brief returns whether the file is temporary, and created in the database process.
     * @details If the file is temporary, the service can remove of modify the file while processing the request.
     * @return true if the BLOB data is temporary
     * @return false otherwise
     */
    [[nodiscard]] virtual bool is_temporary() const noexcept = 0;

    /**
     * @brief disposes temporary resources underlying in this BLOB data.
     * @note If the temporary resources are already disposed, this operation does nothing.
     */
    virtual void dispose() = 0;
};

}
