# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

find_package(Gperftools)
include_directories(${GPERFTOOLS_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/test)

include(CompileProto)
set(TEST_PROTO_FILES
        cord_buf.proto)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test/hdrs)
set(PROTOC_FLAGS ${PROTOC_FLAGS} -I${CMAKE_SOURCE_DIR}/test -I${CMAKE_SOURCE_DIR})
compile_proto(PROTO_HDRS PROTO_SRCS ${CMAKE_BINARY_DIR}/test
        ${CMAKE_BINARY_DIR}/test/hdrs
        ${CMAKE_SOURCE_DIR}/test
        "${TEST_PROTO_FILES}")
add_library(TEST_PROTO_LIB OBJECT ${PROTO_SRCS} ${PROTO_HDRS})

include(require_gtest)
include(require_gmock)

include(require_gtest)
include(require_gmock)
set(GTM_LIB
        ${GTEST_LIB}
        ${GMOCK_LIB}
        )

set(IO_TEST_SRC
        ${PROJECT_SOURCE_DIR}/test/io/cord_buf_test.cc
        )


# fiber_* functions are used in logging.cc, and they need to be marked as
# weak symbols explicitly in Darwin system.
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(DYNAMIC_LIB ${DYNAMIC_LIB}
            "-Wl,-U,_fiber_getspecific"
            "-Wl,-U,_fiber_setspecific"
            "-Wl,-U,_fiber_key_create")
endif ()

file(GLOB_RECURSE TESTING_SOURCES "${PROJECT_SOURCE_DIR}/test/testing/*.cc")

carbin_cc_library(
        NAMESPACE flare
        NAME testing
        SOURCES ${TESTING_SOURCES}
        PUBLIC_LINKED_TARGETS ${CARBIN_DYLINK} ${DYNAMIC_LIB} flare::flare-debug
        PRIVATE_COMPILE_OPTIONS ${CARBIN_DEFAULT_COPTS} -g -O2
        SHARED
)

add_definitions(
        -DUNIT_TEST
        -Dprivate=public
        -Dprotected=public
)
# test_io
carbin_cc_test(
        NAME io_test
        SOURCES ${PROTO_SRCS} io/cord_buf_test.cc
        PUBLIC_LINKED_TARGETS ${GTEST_MAIN_LIB}  ${GTM_LIB} ${CARBIN_DYLINK} ${DYNAMIC_LIB} flare::flare-debug flare::testing
        PRIVATE_COMPILE_OPTIONS ${CARBIN_TEST_COPTS} -g -O2
        VERBOSE
)

add_subdirectory(var)

# fiber tests
add_subdirectory(fiber)

#
add_subdirectory(base)
add_subdirectory(bootstrap)
add_subdirectory(container)
add_subdirectory(files)
add_subdirectory(future)
add_subdirectory(hash)
add_subdirectory(log)
add_subdirectory(memory)
add_subdirectory(strings)
add_subdirectory(io)
add_subdirectory(debugging)
add_subdirectory(times)
add_subdirectory(thread)
add_subdirectory(metrics)

