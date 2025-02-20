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

#include "mock_faulty_service.h"

namespace tateyama::service {

// utilities
static void reply_ok(response* res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_result_only();
    (void) rmsg->mutable_success();

    res->body(response_message.SerializeAsString());
}

using tateyama::api::server::data_channel;
using tateyama::api::server::writer;

// request handlers
void mock_faulty_service::begin(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_begin();
    auto* success = rmsg->mutable_success();
    auto* transaction_handle = success->mutable_transaction_handle();
    transaction_handle->set_handle(12345);

    res->body(response_message.SerializeAsString());
}

void mock_faulty_service::prepare(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_prepare();
    auto* handle = rmsg->mutable_prepared_statement_handle();
    handle->set_handle(6789);

    res->body(response_message.SerializeAsString());
}

void mock_faulty_service::execute_query(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    query(req, res);
}
void mock_faulty_service::execute_prepared_query(std::shared_ptr<request> req, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    query(req, res);
}
void mock_faulty_service::query(std::shared_ptr<request>, std::shared_ptr<response> res) {
    const int size = 2;
    
    std::shared_ptr<data_channel> ch;
    (void) res->acquire_channel("ResultSetName", ch, 1);
    std::shared_ptr<writer> wrt;
    (void) ch->acquire(wrt);

    auto vw = std::make_shared<jogasaki::serializer::value_writer<writer, std::size_t>>(*wrt);

    (void) vw->write_row_begin(size);
    for (auto i = 0; i < size; i++) {
        (void) vw->write_int(i);
    }
    (void) wrt->commit();
#if 0
    (void) vw->write_end_of_contents();

    (void) ch->release(*wrt);
    (void) res->release_channel(*ch);
#endif

    jogasaki::proto::sql::response::Response head{};
    auto *eq = head.mutable_execute_query();
    eq->set_name("ResultSetName");
    auto *meta = eq->mutable_record_meta();
    for (auto i = 0; i < size; i++) {
        auto *column = meta->add_columns();
        column->set_name(std::string("column_") + std::to_string(i));
        column->set_atom_type(jogasaki::proto::sql::common::AtomType::INT8);
    }
    res->body_head(head.SerializeAsString());

#if 0
    std::thread th([res]{
        std::this_thread::sleep_for(std::chrono::seconds(10));

        jogasaki::proto::sql::response::Response response_message{};
        auto *rmsg = response_message.mutable_result_only();
        (void) rmsg->mutable_success();

        res->body(response_message.SerializeAsString());
    });
    th.detach();
#else
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << " ---- finish ----" << std::endl;
#endif
}

void mock_faulty_service::commit(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}
void mock_faulty_service::rollback(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}
void mock_faulty_service::dispose_prepared_statement(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}
void mock_faulty_service::dispose_transaction(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;
    reply_ok(res.get());
}

// does not support with the mock server
static std::string does_not_support(const std::string& name) {
    using namespace std::literals::string_literals;
    return name + " is not supported with the mock server"s;
}

void mock_faulty_service::execute_statement(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_prepare();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_statement"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::execute_prepared_statement(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_prepare();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_prepared_statement"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::get_large_object_data(std::shared_ptr<request>, std::shared_ptr<response> res) {
    std::cout << "======== " << __func__ << " ========" << std::endl;

    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_get_large_object_data();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("get_large_object_data"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::explain(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_explain();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("explain"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::execute_dump(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_result_only();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_dump"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::execute_load(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_execute_result();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("execute_load"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::describe_table(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_describe_table();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support(""));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::batch(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_execute_result();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("batch"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::list_tables(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_list_tables();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("list_tables"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::get_search_path(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_get_search_path();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("get_search_path"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::get_error_info(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_get_error_info();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("get_error_info"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::explain_by_text(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_explain();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("explain_by_text"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}
void mock_faulty_service::extract_statement_info(std::shared_ptr<request>, std::shared_ptr<response> res) {
    jogasaki::proto::sql::response::Response response_message{};
    auto *rmsg = response_message.mutable_extract_statement_info();
    auto* error = rmsg->mutable_error();
    error->set_detail(does_not_support("extract_statement_info"));
    error->set_code(::jogasaki::proto::sql::error::Code::SQL_SERVICE_EXCEPTION);

    res->body(response_message.SerializeAsString());
}

} // namespace tateyama::service
