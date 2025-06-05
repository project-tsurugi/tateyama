/*
 * Copyright 2018-2023 Project Tsurugi.
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
#include <cstdio>
#include <thread>

#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "server_client_base.h"

namespace tateyama::endpoint::ipc {

/*
 *  see https://github.com/google/googletest/issues/1153 for using fork() in GTEST
 */
int wait_for_child_fork(int pid) {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return (-1);
    }
    if (WIFEXITED(status)) { // NOLINT
        return WEXITSTATUS(status); // NOLINT
    }
    return (-2);
}

void server_client_base::wait_client_exit() {
    for (pid_t pid : client_pids_) {
        check_client_exitcode(wait_for_child_fork(pid));
    }
}

void server_client_base::server() {
    tateyama::framework::server sv { tateyama::framework::boot_mode::database_server, cfg_ };
    tateyama::test_utils::add_core_components_for_test(sv);
    sv.add_service(create_server_service());
    assert_true(sv.setup());
    assert_true(sv.start());
    server_startup_end();
    server_elapse_.start();
    wait_client_exit();
    server_elapse_.stop();
    assert_true(sv.shutdown());
}

void server_client_base::server_dump(const std::size_t msg_num, const std::size_t len_sum) {
    std::int64_t msec = server_elapse_.msec();
    double sec = msec / 1000.0;
    double mb_len = len_sum / (1024.0 * 1024.0);
    std::cout << "nproc=" << nproc_;
    std::cout << ", nthread/proc=" << nthread_;
    std::cout << ", elapse=" << std::fixed << std::setprecision(3) << sec << "[sec]";
    std::cout << ", msg_num=" << msg_num;
    std::cout << ", " << std::fixed << std::setprecision(1) << msg_num / sec / 1000.0 << "[Kmsg/sec]";
    std::cout << ", " << std::fixed << std::setprecision(1) << 1000.0 * msec / msg_num << "[usec/msg]";
    std::cout << ", len_sum=" << len_sum;
    std::cout << "=" << std::fixed << std::setprecision(1) << mb_len << "[MB]";
    std::cout << ", speed=" << std::fixed << std::setprecision(1) << mb_len / sec << "[MB/sec]";
    std::cout << std::endl;
}

void server_client_base::client() {
    if (nthread_ == 0) {
        // use main thread, not make another working thread
        client_thread();
        return;
    }
    //
    threads_.clear();
    threads_.reserve(nthread_);
    for (int i = 0; i < nthread_; i++) {
        threads_.emplace_back([this] {
            client_thread();
        });
    }
    for (std::thread &th : threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
}

void server_client_base::start_server_client() {
    server_startup_start(); // call this before fork()
    for (int i = 0; i < nproc_; i++) {
        pid_t pid = fork();
        if (pid > 0) {
            // parent
            client_pids_.push_back(pid);
        } else if (pid == 0) {
            // child: wait server startup and go!
            wait_server_startup_end();
            client();
            client_exit();
            return; // not reached here
        } else {
            assert_failed(10);
        }
    }
    server();
}

void server_client_base::server_startup_start() {
    boost::filesystem::path path = boost::filesystem::temp_directory_path();
    path /= "ipc-test-" + std::to_string(getpid()) + "-" + std::to_string(server_elapse_.now_msec().count());
    lock_filename_ = path.string();
    fd_ = open(lock_filename_.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); // NOLINT
    if (fd_ < 0) {
        perror("open");
        assert_gt(fd_, 0);
    }
    assert_eq(0, flock(fd_, LOCK_EX));
}

void server_client_base::server_startup_end() {
    assert_gt(fd_, 0);
    assert_eq(0, flock(fd_, LOCK_UN));
    assert_eq(0, close(fd_));
    assert_eq(0, unlink(lock_filename_.c_str()));
    fd_ = 0;
}

void server_client_base::wait_server_startup_end() {
    assert_gt(fd_, 0);
    assert_eq(0, close(fd_));
    fd_ = open(lock_filename_.c_str(), O_RDONLY); // NOLINT
    if (fd_ < 0 && errno == ENOENT) {
        // Server startup has already finished and the file has already deleted.
        // It's not necessary to wait.
        return;
    }
    assert_gt(fd_, 0);
    // wait until server_startup_end() unlocks the file
    assert_eq(0, flock(fd_, LOCK_SH));
    assert_eq(0, flock(fd_, LOCK_UN));
    assert_eq(0, close(fd_));
    fd_ = 0;
}

} // namespace tateyama::endpoint::ipc
