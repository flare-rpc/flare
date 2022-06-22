

file(GLOB TEST_VARIABLE_SRCS "variable_*_test.cc")
carbin_cc_test(
        NAME variable_test
        SOURCES ${TEST_VARIABLE_SRCS}
        PUBLIC_LINKED_TARGETS ${GTEST_MAIN_LIB}  ${GTM_LIB} ${CARBIN_DYLINK} ${DYNAMIC_LIB} ${GPERFTOOLS_LIBRARIES} flare::flare-debug flare::testing
        PRIVATE_COMPILE_OPTIONS ${CARBIN_TEST_COPTS} -g -O2
        VERBOSE
)