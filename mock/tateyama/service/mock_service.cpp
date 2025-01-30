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

#include <glog/logging.h>

#include <takatori/util/exception.h>
#include <jogasaki/proto/sql/request.pb.h>
#include <jogasaki/proto/sql/response.pb.h>

#include "mock_service.h"

using takatori::util::throw_exception;

namespace tateyama::service {

namespace framework = tateyama::framework;

constexpr static std::string_view log_location_prefix = "/:tateyama:mock_service:logging ";

framework::component::id_type mock_service::id() const noexcept {
    return tag;
}

bool mock_service::setup(framework::environment&) {
    return true;
}

bool mock_service::start(framework::environment&) {
    return true;
}

bool mock_service::shutdown(framework::environment&) {
    return true;
}

mock_service::~mock_service() {
    VLOG(log_info) << "/:tateyama:lifecycle:component:<dtor> " << component_label;
}

bool mock_service::operator()(std::shared_ptr<request> req, [[maybe_unused]] std::shared_ptr<response> res) {
    jogasaki::proto::sql::request::Request proto_req { };
    res->session_id(req->session_id());
    auto s = req->payload();
    if (!proto_req.ParseFromArray(s.data(), static_cast<int>(s.size()))) {
        throw_exception(std::logic_error{"proto_req.ParseFromArray failed"});
    }
    switch (proto_req.request_case()) {
    case jogasaki::proto::sql::request::Request::kBegin:
        begin(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kPrepare:
        prepare(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExecuteStatement:
        execute_statement(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExecuteQuery:
        execute_query(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExecutePreparedStatement:
        execute_prepared_statement(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExecutePreparedQuery:
        execute_prepared_query(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kCommit:
        commit(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kRollback:
        rollback(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kDisposePreparedStatement:
        dispose_prepared_statement(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExplain:
        explain(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExecuteDump:
        execute_dump(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExecuteLoad:
        execute_load(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kDescribeTable:
        describe_table(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kBatch:
        batch(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kListTables:
        list_tables(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kGetSearchPath:
        get_search_path(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kGetErrorInfo:
        get_error_info(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kDisposeTransaction:
        dispose_transaction(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExplainByText:
        explain_by_text(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kExtractStatementInfo:
        extract_statement_info(req, res);
        break;
    case jogasaki::proto::sql::request::Request::kGetLargeObjectData:
        get_large_object_data(req, res);
        break;
    default:
        throw_exception(std::logic_error{"illegal command_case"});
    }
    return true;
}

std::string_view mock_service::label() const noexcept {
    return component_label;
}

} // namespace tateyama::service
