/*
 * Copyright 2019-2019 tsurugi project.
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
#include "ipc_provider.h"

/**
 * @brief add ipc endpoint to the component registry
 * @note This should be done in .cpp file. In the header file,
 * the initialization of inline variable is sometimes omitted by optimization.
 */
register_component(endpoint, tateyama::api::endpoint::provider, ipc_endpoint, tateyama::server::ipc_provider::create);  //NOLINT
