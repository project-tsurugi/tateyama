/*
 * Copyright 2018-2020 tsurugi project.
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

#include <iostream>
#include <iomanip>

namespace tateyama::utils {

namespace details {

class boolalpha {
public:
    explicit boolalpha(bool value) :
        value_(value)
    {}

    void write(std::ostream& stream) const {
        auto flags = stream.setf(std::ios_base::boolalpha);
        stream << value_;
        stream.setf(flags, std::ios_base::basefield);
    }

private:
    bool value_;
};

std::ostream& operator<<(std::ostream& stream, details::boolalpha const& value) {
    value.write(stream);
    return stream;
}

} // namespace details

details::boolalpha boolalpha(bool value) {
    return details::boolalpha(value);
}

}

