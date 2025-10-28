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
#include <filesystem>
#include <vector>

namespace tateyama::grpc {

class blob_session_impl;

/**
 * @brief Represents a session context for BLOB operations.
 */
class blob_session {
public:
    /// @brief the session ID type.
    using session_id_type = std::uint64_t;

    /// @brief the transaction ID type.
    using transaction_id_type = std::uint64_t;

    /// @brief the BLOB ID type.
    using blob_id_type = std::uint64_t;

    /// @brief the BLOB file path type.
    using blob_path_type = std::filesystem::path;

    /// @brief the BLOB tag type.
    using blob_tag_type = std::uint64_t;

    /**
     * @brief returns the ID of this session.
     * @return the session ID
     */
    [[nodiscard]] session_id_type session_id() const noexcept;

    /**
     * @brief returns whether this session is valid.
     * @detail if dispose() has been called, this method returns false.
     * @return true if this session is valid
     * @return false otherwise.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief dispose this session and release all resources associated with it.
     * @note After calling this method, the session becomes invalid and cannot be used anymore.
     * @attention please ensure to call this method to avoid resource leaks before this object is destroyed.
     */
    void dispose();

    /**
     * @brief adds a BLOB data file path to this session.
     * @param path the path to the BLOB data file to add.
     * @return the BLOB ID assigned to the added BLOB data file.
     * @attention undefined behavior occurs if the path is already added in this session.
     */
    [[nodiscard]] blob_id_type add(blob_path_type path);

    /**
     * @brief find the BLOB data file path associated with the given BLOB ID.
     * @param blob_id the BLOB ID to retrieve
     * @return the path to the BLOB data file if found
     * @return otherwise, std::nullopt.
     */
    [[nodiscard]] std::optional<blob_path_type> find(blob_id_type blob_id) const;

    /**
     * @brief returns a list of added BLOB IDs in this session.
     * @return the list of added BLOB IDs.
     */
    [[nodiscard]] std::vector<blob_id_type> entries() const;

    /**
     * @brief removes the BLOB data file associated with the given BLOB IDs from this session.
     * @details if there is no BLOB data file associated with the given BLOB ID, this method does nothing.
     * @param blob_ids_begin the beginning of the range of BLOB IDs to remove.
     * @param blob_ids_end the end of the range of BLOB IDs to remove.
     */
    template<class Iter>
    void remove(Iter blob_ids_begin, Iter blob_ids_end);

    /**
      * @brief computes a tag value for the given BLOB ID.
      * @param blob_id the BLOB ID to compute the tag for.
      * @return the computed tag value.
      */
    [[nodiscard]] blob_tag_type compute_tag(blob_id_type blob_id) const;

    /**
      * @brief create a new object.
      */
    blob_session(std::unique_ptr<blob_session_impl, void(*)(blob_session_impl*)> impl);

private:
    std::unique_ptr<blob_session_impl, void(*)(blob_session_impl*)> impl_;
};

}
