# Tateyama - Application Infrastructure for Tsurugi

## Requirements

* CMake `>= 3.16`
* C++ Compiler `>= C++17`
* and see *Dockerfile* section

```sh
# retrieve third party modules
git submodule update --init --recursive
```

### Dockerfile

```dockerfile
FROM ubuntu:22.04

RUN apt update -y && apt install -y git build-essential cmake ninja-build libboost-filesystem-dev libboost-system-dev libboost-container-dev libboost-thread-dev libboost-stacktrace-dev libgoogle-glog-dev libgflags-dev doxygen libtbb-dev libnuma-dev libssl-dev libjson-c-dev libjwt-dev
```

optional packages:

* `doxygen`
* `graphviz`
* `clang-tidy-14`

### Install modules

#### tsurugidb modules

This requires below [tsurugidb](https://github.com/project-tsurugi/tsurugidb) modules to be installed.

* [takatori](https://github.com/project-tsurugi/takatori)  (for takatori::util functionalities)
* [sharksfin](https://github.com/project-tsurugi/sharksfin)

#### moodycamel::ConcurrentQueue

```sh
mkdir -p build-third_party/concurrentqueue
cd build-third_party/concurrentqueue
cmake -G Ninja -DCMAKE_INSTALL_PREFIX=[/path/to/install-prefix] ../../third_party/concurrentqueue
cmake --build . --target install
```

see https://github.com/cameron314/concurrentqueue

#### restclient-cpp

```sh
mkdir -p build-third_party/restclient-cpp
cd build-third_party/restclient-cpp
cmake -G Ninja -DCMAKE_INSTALL_PREFIX=[/path/to/install-prefix] ../../third_party/restclient-cpp
cmake --build . --target install
```

see https://github.com/mrtazz/restclient-cpp

## How to build

```sh
mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

available options:
* `-DCMAKE_INSTALL_PREFIX=<installation directory>` - change install location
* `-DCMAKE_PREFIX_PATH=<installation directory>` - indicate prerequisite installation directory
* `-DCMAKE_IGNORE_PATH="/usr/local/include;/usr/local/lib/"` - specify the libraries search paths to ignore. This is convenient if the environment has conflicting version installed on system default search paths. (e.g. gflags in /usr/local)
* `-DBUILD_SHARED_LIBS=OFF` - create static libraries instead of shared libraries
* `-DBUILD_TESTS=OFF` - don't build test programs
* `-DBUILD_EXAMPLES=OFF` - don't build example programs
* `-DBUILD_BENCHMARK=OFF` - don't build benchmark programs
* `-DBUILD_DOCUMENTS=OFF` - don't build documents by doxygen
* `-DBUILD_STRICT=OFF` - don't treat compile warnings as build errors
* `-DINSTALL_EXAMPLES=ON` - install example applications
* `-DSHARKSFIN_IMPLEMENTATION=<implementation name>` - switch sharksfin implementation. Available options are `memory` and `shirakami` (default: `shirakami`)
* `-DENABLE_ALTIMETER=ON` - turn on the `altimeter logging`.
* `-DMC_QUEUE=ON` - use moody camel queue instead of tbb queue to store tasks in tateyama task scheduler.
* `-DENABLE_DEBUG_SERVICE=OFF` - turn off the `debug service`.
* for debugging only
  * `-DENABLE_SANITIZER=OFF` - disable sanitizers (requires `-DCMAKE_BUILD_TYPE=Debug`)
  * `-DENABLE_UB_SANITIZER=ON` - enable undefined behavior sanitizer (requires `-DENABLE_SANITIZER=ON`)
  * `-DENABLE_COVERAGE=ON` - enable code coverage analysis (requires `-DCMAKE_BUILD_TYPE=Debug`)
  * `-DTRACY_ENABLE=ON` - enable tracy profiler for multi-thread debugging. See section below.
* for developper only
  * `-DBUILD_MOCK=ON` - build a mock server

### install

```sh
cmake --build . --target install
```

### run tests

Execute the test as below:
```sh
ctest -V
```

### generate documents

```sh
cmake --build . --target doxygen
```

### Customize logging setting
You can customize logging in the same way as sharksfin. See sharksfin [README.md](https://github.com/project-tsurugi/sharksfin/blob/master/README.md#customize-logging-setting) for more details.

```sh
GLOG_minloglevel=0 ./group-cli --minimum
```

### Multi-thread debugging/profiling with Tracy

You can use [Tracy Profiler](https://github.com/wolfpld/tracy) to graphically display the threads operations and improve printf debug by printing messages on the tooltips on the Tracy profiler UI.
By setting cmake build option `-DTRACY_ENABLE=ON`, TracyClient.cpp file is added to the build and tracing macros are enabled.

Prerequirement:

1. ensure tracy code is located under `third_party/tracy` directory.
```
git submodule update --init third_party/tracy
```

2. include common.h at the top of files that requires tracing.
```
#include <tateyama/common.h>
```

3. Put `trace_scope` at the beginning of the scope to trace, or use other tracing functions defined in common.h.

## License

[Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)

