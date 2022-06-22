

file(GLOB_RECURSE VARIABLE_SOURCES "${PROJECT_SOURCE_DIR}/flare/variable/*.cc")
file(GLOB_RECURSE FIBER_SOURCES "${PROJECT_SOURCE_DIR}/flare/fiber/*.cc")
file(GLOB_RECURSE FLARE_BASE_SRC "${PROJECT_SOURCE_DIR}/flare/base/*.cc")
file(GLOB_RECURSE FLARE_CONTAINER_SRC "${PROJECT_SOURCE_DIR}/flare/container/*.cc")
file(GLOB_RECURSE FLARE_HASH_SRC "${PROJECT_SOURCE_DIR}/flare/hash/*.cc")
file(GLOB_RECURSE FLARE_MEMORY_SRC "${PROJECT_SOURCE_DIR}/flare/memory/*.cc")
file(GLOB_RECURSE FLARE_IO_SRC "${PROJECT_SOURCE_DIR}/flare/io/*.cc")
file(GLOB_RECURSE FLARE_STRINGS_SRC "${PROJECT_SOURCE_DIR}/flare/strings/*.cc")
file(GLOB_RECURSE FLARE_DEBUG_SRC "${PROJECT_SOURCE_DIR}/flare/debugging/*.cc")
file(GLOB_RECURSE FLARE_LOG_SRC "${PROJECT_SOURCE_DIR}/flare/log/*.cc")
file(GLOB_RECURSE FLARE_BOOTSTRAP_SRC "${PROJECT_SOURCE_DIR}/flare/bootstrap/*.cc")
file(GLOB_RECURSE FLARE_TIMES_SRC "${PROJECT_SOURCE_DIR}/flare/times/*.cc")
file(GLOB_RECURSE FLARE_FILES_SRC "${PROJECT_SOURCE_DIR}/flare/files/*.cc")
file(GLOB_RECURSE FLARE_THREAD_SRC "${PROJECT_SOURCE_DIR}/flare/thread/*.cc")
file(GLOB_RECURSE FLARE_METRICS_SRC "${PROJECT_SOURCE_DIR}/flare/metrics/*.cc")

set(FLARE_SRC
        "${PROJECT_SOURCE_DIR}/flare/base/dynamic_annotations/dynamic_annotations.c"
        ${FLARE_BASE_SRC}
        ${FLARE_CONTAINER_SRC}
        ${FLARE_HASH_SRC}
        ${FLARE_MEMORY_SRC}
        ${FLARE_IO_SRC}
        ${FLARE_STRINGS_SRC}
        ${FLARE_DEBUG_SRC}
        ${FLARE_LOG_SRC}
        ${FLARE_BOOTSTRAP_SRC}
        ${FLARE_HASH_SRC}
        ${FLARE_TIMES_SRC}
        ${FLARE_FILES_SRC}
        ${FLARE_THREAD_SRC}
        ${FLARE_METRICS_SRC}
        ${VARIABLE_SOURCES}
        ${FIBER_SOURCES}
        )

#
#if (APPLE)
#    find_library(CoreFoundation CoreFoundation)
#    carbin_cc_library(
#            NAME
#            abel
#            SRCS
#            ${ABEL_SRC}
#            COPTS
#            ${CARBIN_DEFAULT_COPTS}
#            DEPS
#                ${CoreFoundation}
#                pthread
#            PUBLIC
#    )
#elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
#    carbin_cc_library(
#            NAME
#            abel
#            SRCS
#            ${ABEL_SRC}
#            COPTS
#            ${CARBIN_DEFAULT_COPTS}
#            DEPS
#            rt
#            pthread
#            PUBLIC
#    )
#endif ()
#

# carbin_cc_library(  NAME myLibrary
#                  NAMESPACE myNamespace
#                  SOURCES
#                       myLib.cpp
#                       myLib_functions.cpp
#                  HEADERS
#                        mylib.h
#                  PUBLIC_DEFINITIONS
#                     USE_DOUBLE_PRECISION=1
#                  PRIVATE_DEFINITIONS
#                     DEBUG_VERBOSE
#                  PUBLIC_INCLUDE_PATHS
#                     ${CMAKE_SOURCE_DIR}/mylib/include
#                  PRIVATE_INCLUDE_PATHS
#                     ${CMAKE_SOURCE_DIR}/include
#                  PRIVATE_LINKED_TARGETS
#                     Threads::Threads
#                  PUBLIC_LINKED_TARGETS
#                     Threads::Threads
#                  LINKED_TARGETS
#                     Threads::Threads
#                  PUBLIC
#                  SHARED

carbin_cc_library(
        NAMESPACE flare
        NAME flare
        SOURCES ${FLARE_SRC}
        PUBLIC_LINKED_TARGETS ${CARBIN_DYLINK} ${DYNAMIC_LIB}
        PRIVATE_COMPILE_OPTIONS ${CARBIN_DEFAULT_COPTS}
        PUBLIC
        SHARED
)

if (ENABLE_TESTING)
    list(REMOVE_ITEM FLARE_SRC ${PROJECT_SOURCE_DIR}/flare/variable/default_variables.cc)
    carbin_cc_library(
            NAMESPACE flare
            NAME flare-debug
            SOURCES ${FLARE_SRC}
            PUBLIC_LINKED_TARGETS ${CARBIN_DYLINK} ${DYNAMIC_LIB}
            PRIVATE_COMPILE_OPTIONS ${CARBIN_DEFAULT_COPTS} -g -O2 -DVARIABLE_NOT_LINK_DEFAULT_VARIABLES
            SHARED
    )

endif ()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/flare
        DESTINATION ${CMAKE_INSTALL_PREFIX}/flare
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
        )


