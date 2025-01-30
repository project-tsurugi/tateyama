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
#include <iostream>
#include <thread>
#include <chrono>

#include <jogasaki/proto/sql/request.pb.h>
#include <jogasaki/proto/sql/response.pb.h>
#include <jogasaki/proto/sql/common.pb.h>
#include <jogasaki/proto/sql/error.pb.h>
#include <jogasaki/serializer/value_writer.h>

#include "mock_service.h"
#include "test_pattern.h"

namespace tateyama::service {

test_pattern pattern{};

// utilities
static void reply_ok(response* res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_result_only();
    (void) rmsg->mutable_success();

    res->body(response_message.SerializeAsString());
}

static void dump_lob(request* req, const std::string& name, const std::string& local_path) {
    std::cout << "req->has_blob(\"" << name << "\") = " << req->has_blob(name) << std::endl;
    if (!local_path.empty()) {
        std::cout << "local_path =" << local_path << std::endl;
    }
    try {
        auto& blob_info = req->get_blob(name);
        std::cout << "blob_info for " << name
                  << " : channel_name = " << blob_info.channel_name()
                  << " : path = " << blob_info.path().string()
                  << " : temporary = " << blob_info.is_temporary()
                  << std::endl;
    } catch (std::runtime_error &ex) {
        std::cout << "can't find blob_info for BlobChannelUp" << std::endl;
    }
}

using tateyama::api::server::data_channel;
using tateyama::api::server::writer;

// request handlers
void mock_service::begin(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_begin();
    auto* success = rmsg->mutable_success();
    auto* transaction_handle = success->mutable_transaction_handle();
    transaction_handle->set_handle(12345);

    res->body(response_message.SerializeAsString());
}

void mock_service::prepare(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_prepare();
    auto* handle = rmsg->mutable_prepared_statement_handle();
    handle->set_handle(6789);

    res->body(response_message.SerializeAsString());
}

void mock_service::execute_prepared_statement(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    jogasaki::proto::sql::request::Request request_message{};
    request_message.ParseFromString(std::string(req->payload()));
    auto& parameters = request_message.execute_prepared_statement().parameters();
    bool need_line_break{};
    for (auto&& e: parameters) {
        if (need_line_break) {
            std::cout << "========" << std::endl;
        }
        auto value_case = e.value_case();
        if (value_case == jogasaki::proto::sql::request::Parameter::ValueCase::kBlob) {
            auto value = e.blob();
            dump_lob(req.get(), value.channel_name(), value.data_case() == jogasaki::proto::sql::common::Blob::DataCase::kLocalPath ? value.local_path() : "");
        }
        if (value_case == jogasaki::proto::sql::request::Parameter::ValueCase::kClob) {
            auto value = e.clob();
            dump_lob(req.get(), value.channel_name(), value.data_case() == jogasaki::proto::sql::common::Clob::DataCase::kLocalPath ? value.local_path() : "");
        }
        need_line_break = true;
    }
        
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_execute_result();
    auto* success = rmsg->mutable_success();
    auto* counter = success->add_counters();
    counter->set_type(jogasaki::proto::sql::response::ExecuteResult::INSERTED_ROWS);
    counter->set_value(123);

    res->body(response_message.SerializeAsString());
}

void mock_service::execute_prepared_query(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;

    std::shared_ptr<data_channel> ch;
    (void) res->acquire_channel("ResultSetName", ch, 1);
    std::shared_ptr<writer> wrt;
    (void) ch->acquire(wrt);

    auto vw = std::make_shared<jogasaki::serializer::value_writer<writer, std::size_t>>(*wrt);

    (void) vw->write_row_begin(pattern.size());
    pattern.foreach_blob([vw](std::size_t lob_id, std::string&, std::string&, std::string&){
        (void) vw->write_blob(1, lob_id);
    });
    pattern.foreach_clob([vw](std::size_t lob_id, std::string&, std::string&, std::string&){
        (void) vw->write_clob(1, lob_id);
    });
    (void) wrt->commit();
    (void) vw->write_end_of_contents();

    (void) ch->release(*wrt);
    (void) res->release_channel(*ch);

    jogasaki::proto::sql::response::Response head{};
    auto *eq = head.mutable_execute_query();
    eq->set_name("ResultSetName");
    auto *meta = eq->mutable_record_meta();
    pattern.foreach_blob([meta](std::size_t, std::string& name, std::string&, std::string&){
        auto *column = meta->add_columns();
        column->set_name(name);
        column->set_atom_type(jogasaki::proto::sql::common::AtomType::BLOB);
    });
    pattern.foreach_clob([meta](std::size_t, std::string& name, std::string&, std::string&){
        auto *column = meta->add_columns();
        column->set_name(name);
        column->set_atom_type(jogasaki::proto::sql::common::AtomType::CLOB);
    });
    res->body_head(head.SerializeAsString());

    pattern.foreach_blob([res](std::size_t, std::string&, std::string& blob_channel, std::string& file_name){
        res->add_blob(std::make_unique<blob_info_for_test>(blob_channel, std::filesystem::path(file_name), false));
    });
    pattern.foreach_clob([res](std::size_t, std::string&, std::string& clob_channel, std::string& file_name){
        res->add_blob(std::make_unique<blob_info_for_test>(clob_channel, std::filesystem::path(file_name), false));
    });

    std::thread th([res]{
        std::this_thread::sleep_for(std::chrono::seconds(1));

        jogasaki::proto::sql::response::Response response_message{};
        auto *rmsg = response_message.mutable_result_only();
        (void) rmsg->mutable_success();

        res->body(response_message.SerializeAsString());
    });
    th.detach();
}

void mock_service::get_large_object_data(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;

    jogasaki::proto::sql::request::Request request_message{};
    request_message.ParseFromString(std::string(req->payload()));
    auto& ref = request_message.get_large_object_data().reference();
    std::cout << "reference = " << ref.provider() << ":" << ref.object_id() << std::endl;

    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_get_large_object_data();
    if (auto name_opt = pattern.find(ref.object_id()); name_opt) {
        auto* success = rmsg->mutable_success();
        success->set_channel_name(name_opt.value());
    } else {
        auto* error = rmsg->mutable_error();
        error->set_detail("invalid object id");
        error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);
    }
    res->body(response_message.SerializeAsString());
}

void mock_service::commit(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}
void mock_service::rollback(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}
void mock_service::dispose_prepared_statement(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}
void mock_service::dispose_transaction(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}

// does not support with the mock server
std::string does_not_support(const std::string& name) {
    using namespace std::literals::string_literals;
    return name + " is not supported with the mock server"s;
}

void mock_service::execute_statement(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_prepare();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_statement"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::execute_query(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_execute_result();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_query"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::explain(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_explain();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("explain"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::execute_dump(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_result_only();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_dump"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::execute_load(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_execute_result();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_load"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::describe_table(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_describe_table();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support(""));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::batch(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_execute_result();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("batch"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::list_tables(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_list_tables();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("list_tables"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::get_search_path(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_get_search_path();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("get_search_path"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::get_error_info(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_get_error_info();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("get_error_info"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::explain_by_text(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_explain();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("explain_by_text"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_service::extract_statement_info(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_extract_statement_info();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("extract_statement_info"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}

} // namespace tateyama::service
