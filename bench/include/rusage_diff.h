/*
 * Copyright 2018-2023 tsurugi project.
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

#include <sys/time.h>
#include <sys/resource.h>

class rusage_diff {
public:
    rusage_diff(bool server) :
            who_(server ? RUSAGE_SELF : RUSAGE_CHILDREN) {
    }

    void start() {
        get_rusage(&start_);
    }

    void end() {
        get_rusage(&end_);
    }

    struct rusage& get_diff() {
        minus(start_.ru_utime, end_.ru_utime, diff_.ru_utime);
        minus(start_.ru_stime, end_.ru_stime, diff_.ru_stime);
        diff_.ru_nvcsw = end_.ru_nvcsw - start_.ru_nvcsw;
        diff_.ru_nivcsw = end_.ru_nivcsw - start_.ru_nivcsw;
        return diff_;
    }

private:
    void get_rusage(struct rusage *usage) {
        getrusage(who_, usage);
    }

    static const long sec2micro = 1'000'000;

    long tv2long(struct timeval &tv) {
        return (sec2micro * tv.tv_sec + tv.tv_usec);
    }

    void long2tv(long t, struct timeval &tv) {
        tv.tv_sec = (t / sec2micro);
        tv.tv_usec = (t % sec2micro);
    }

    void minus(struct timeval &start, struct timeval &end, struct timeval &result) {
        long t1 = tv2long(start);
        long t2 = tv2long(end);
        long t3 = t2 - t1;
        long2tv(t3, result);
    }

    int who_ { };
    struct rusage start_ { };
    struct rusage end_ { };
    struct rusage diff_ { };
};
