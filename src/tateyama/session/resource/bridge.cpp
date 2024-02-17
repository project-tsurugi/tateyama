/*
 * Copyright 2018-2024 Project Tsurugi.
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

#include <chrono>
#include <boost/lexical_cast.hpp>

#include "tateyama/session/resource/bridge.h"

namespace tateyama::session::resource {

using namespace framework;

component::id_type bridge::id() const noexcept {
    return tag;
}

bool bridge::setup(environment&) {
    return true;
}

bool bridge::start(environment&) {
    return true;
}

bool bridge::shutdown(environment&) {
    return true;
}

bridge::~bridge() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
};

std::string_view bridge::label() const noexcept {
    return component_label;
}

static void set_entry(::tateyama::proto::session::response::SessionInfo* entry, const tateyama::api::server::session_info& info) {
    std::string sid{":"};
    sid += std::to_string(info.id());
    entry->set_session_id(sid);
    entry->set_label(std::string(info.label()));
    entry->set_application(std::string(info.application_name()));
    entry->set_user(std::string(info.user_name()));
    entry->set_start_at(std::chrono::duration_cast<std::chrono::milliseconds>(info.start_at().time_since_epoch()).count());
    entry->set_connection_type(std::string(info.connection_type_name()));
    entry->set_connection_info(std::string(info.connection_information()));
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::find_only_one_session(std::string_view session_specifier, session_context::numeric_id_type& numeric_id) {
    if (session_specifier.at(0) == ':') {
        try {
            numeric_id = std::stol(std::string(session_specifier.data() + 1, session_specifier.length() -1));
            return std::nullopt;
        } catch (std::exception &ex) {
            return tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN;
        }
    }
    auto v = sessions_core_.container_.enumerate_numeric_ids(session_specifier);
    if (v.empty()) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;
    }
    if (v.size() > 1) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_AMBIGUOUS;
    }
    numeric_id = v.at(0);
    return std::nullopt;
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::list(::tateyama::proto::session::response::SessionList_Success* mutable_success) {
    sessions_core_.container_.foreach([mutable_success](const std::shared_ptr<session_context>& entry){set_entry(mutable_success->add_entries(), entry->info());});
    return std::nullopt;
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::get(std::string_view session_specifier, ::tateyama::proto::session::response::SessionGet_Success* mutable_success) {
    session_context::numeric_id_type numeric_id{};
    try {
        auto opt = find_only_one_session(session_specifier, numeric_id);
        if (opt) {
            return opt.value();
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_INVALID_VALUE;
    }

    std::shared_ptr<session_context> context{};
    try {
        context = sessions_core_.container_.find_session(numeric_id);
        if (context) {
            set_entry(mutable_success->mutable_entry(), context->info());
            return std::nullopt;
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_INVALID_VALUE;
    }
    return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;        
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::shutdown(std::string_view session_specifier, [[maybe_unused]] shutdown_request_type type) {
    session_context::numeric_id_type numeric_id{};
    try {
        auto opt = find_only_one_session(session_specifier, numeric_id);
        if (opt) {
            return opt.value();
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;
    }

    std::shared_ptr<session_context> context{};
    try {
        context = sessions_core_.container_.find_session(numeric_id);
        if (context) {
            auto rv = context->shutdown_request(type);
            if (rv) {
                return std::nullopt;
            }
            return tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN;  // FIXME assign suitable code
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_INVALID_VALUE;
    }
    return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;        
}

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::set_valiable(std::string_view session_specifier, std::string_view name, std::string_view value) {
    session_context::numeric_id_type numeric_id{};
    try {
        auto opt = find_only_one_session(session_specifier, numeric_id);
        if (opt) {
            return opt.value();
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;
    }

    std::shared_ptr<session_context> context{};
    try {
        context = sessions_core_.container_.find_session(numeric_id);
        if (context) {
            auto& vs = context->variables();
            if (!vs.exists(name)) {
                return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_NOT_DECLARED;
            }
            auto type = vs.type(name);
            if (!type) {
                return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_NOT_DECLARED;
            }
            switch(type.value()) {
            case session_variable_type::boolean:
            {
                try {
                    bool v = boost::lexical_cast<bool>(value);
                    if (vs.set(name, v)) {
                        return std::nullopt;
                    }
                    break;
                } catch (boost::bad_lexical_cast const &ex) {
                    return tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN; // FIXME
                }
            }
            case session_variable_type::signed_integer:
            {
                try {
                    std::int64_t v = std::stol(std::string(value));
                    if (vs.set(name, v)) {
                        return std::nullopt;
                    }
                    break;
                } catch (std::exception const &ex) {
                    return tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN; // FIXME
                }
            }
            case session_variable_type::unsigned_integer:
            {
                try {
                    std::uint64_t v = std::stoul(std::string(value));
                    if (vs.set(name, v)) {
                        return std::nullopt;
                    }
                    break;
                } catch (std::exception const &ex) {
                    return tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN; // FIXME
                }
            }
            case session_variable_type::string:
            {
                if (vs.set(name, std::string(value))) {
                    return std::nullopt;
                }
                break;
            }
            default:
                break;
            }
            return tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN;
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_INVALID_VALUE;
    }
    return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;        
}

class value {
public:
    explicit value(::tateyama::proto::session::response::SessionGetVariable_Success* mutable_success) : mutable_success_(mutable_success) {
    }
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> operator()([[maybe_unused]] const std::monostate& data) {
        return tateyama::proto::session::diagnostic::ErrorCode::UNKNOWN;
    }
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> operator()(const bool data) {
        mutable_success_->set_bool_value(data);
        return std::nullopt;
    }
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> operator()(const std::int64_t& data) {
        mutable_success_->set_signed_integer_value(data);
        return std::nullopt;
    }
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> operator()(const std::uint64_t& data) {
        mutable_success_->set_unsigned_integer_value(data);
        return std::nullopt;
    }
    std::optional<tateyama::proto::session::diagnostic::ErrorCode> operator()(const std::string& data) {
        mutable_success_->set_string_value(data);
        return std::nullopt;
    }
private:
    ::tateyama::proto::session::response::SessionGetVariable_Success* mutable_success_;
};

std::optional<tateyama::proto::session::diagnostic::ErrorCode> bridge::get_valiable(std::string_view session_specifier, std::string_view name, ::tateyama::proto::session::response::SessionGetVariable_Success* mutable_success) {
    session_context::numeric_id_type numeric_id{};
    try {
        auto opt = find_only_one_session(session_specifier, numeric_id);
        if (opt) {
            return opt.value();
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;
    }

    mutable_success->set_name(std::string(name));
    auto& vds = sessions_core_.variable_declarations();
    if (auto* vd = vds.find(name); vd) {
        mutable_success->set_description(vd->description());
    }

    std::shared_ptr<session_context> context{};
    try {
        context = sessions_core_.container_.find_session(numeric_id);
        if (context) {
            auto& vs = context->variables();
            if (!vs.exists(name)) {
                return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_NOT_DECLARED;
            }
            auto type = vs.type(name);
            if (!type) {
                return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_NOT_DECLARED;
            }
            auto res = std::visit(value(mutable_success), vs.get(name));
            if (!res) {
                return std::nullopt;
            }
            return res.value();
        }
    } catch (std::invalid_argument &ex) {
        return tateyama::proto::session::diagnostic::ErrorCode::SESSION_VARIABLE_INVALID_VALUE;
    }
    return tateyama::proto::session::diagnostic::ErrorCode::SESSION_NOT_FOUND;        
}

bool bridge::register_session(std::shared_ptr<session_context> const& session) {
    return sessions_core_.container_.register_session(session);
}

tateyama::session::resource::sessions_core const& bridge::sessions_core() const noexcept {
    return sessions_core_;
}

}
