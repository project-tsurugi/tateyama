/*
 * Copyright 2018-2026 Project Tsurugi.
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

#include <tateyama/status/resource/core.h>

namespace tateyama::status_info::resource {

class resource_status_adapter {
public:
    resource_status_adapter(const std::string& file_name, std::size_t size)
        : mem_(std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, file_name.c_str(), size)),
          file_name_(file_name),
          resource_status_memory_(mem_->construct<resource_status_memory::resource_status>(std::string(resource_status_memory::area_name).c_str())(mem_->get_segment_manager())) {
    }
    ~resource_status_adapter() {
        try {
            mem_->destroy<resource_status_memory::resource_status>(std::string(resource_status_memory::area_name).c_str());
            boost::interprocess::shared_memory_object::remove(file_name_.c_str());
        } catch (std::exception& e) {
            LOG(WARNING) << e.what();
        }
    }

    resource_status_adapter(resource_status_adapter const& other) = delete;
    resource_status_adapter& operator=(resource_status_adapter const& other) = delete;
    resource_status_adapter(resource_status_adapter&& other) noexcept = delete;
    resource_status_adapter& operator=(resource_status_adapter&& other) noexcept = delete;

    resource_status_memory* resource_status_memory_address() {
        return &resource_status_memory_;
    }

private:
    std::unique_ptr<boost::interprocess::managed_shared_memory> mem_;
    std::string file_name_;
    resource_status_memory resource_status_memory_;
};

} // namespace tateyama::status_info::resource
