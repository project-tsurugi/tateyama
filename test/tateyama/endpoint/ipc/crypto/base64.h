/*
 * Copyright 2022-2024 Project Tsurugi.
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

#include <sstream>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

namespace tateyama::endpoint::ipc::crypto {

using namespace boost::archive::iterators;
using InputItr  = std::istreambuf_iterator<char>;
using OutputItr = std::ostream_iterator<char>;
using EncodeItr = boost::archive::iterators::base64_from_binary<boost::archive::iterators::transform_width<InputItr, 6, 8>>;
using DecodeItr = boost::archive::iterators::transform_width<boost::archive::iterators::binary_from_base64<InputItr>, 8, 6, char>;

template <typename InputStream, typename OutputStream>
static inline OutputStream& encode(InputStream& is, OutputStream& os) {
    std::copy(static_cast<EncodeItr>(InputItr(is)), static_cast<EncodeItr>(InputItr()), OutputItr(os));
    return os;
}

template <typename InputStream, typename OutputStream>
static inline OutputStream& decode(InputStream& is, OutputStream& os) {
    std::copy(static_cast<DecodeItr>(InputItr(is)), static_cast<DecodeItr>(InputItr()), OutputItr(os));
    return os;
}

static std::string base64_encode(const std::string& input) {
    std::stringstream ssi;
    std::stringstream sso;
    ssi << input;
    encode(ssi, sso);
    return sso.str();
}

static std::string base64_decode(const std::string& input) {
    std::stringstream ssi;
    std::stringstream sso;
    ssi << input;
    decode(ssi, sso);
    return sso.str();
}

} // namespace tateyama::endpoint::ipc::crypto
