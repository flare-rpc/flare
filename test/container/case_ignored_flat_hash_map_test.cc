//
// Created by liyinbin on 2022/5/15.
//

#include <string>
#include "flare/container/flat_hash_map.h"
#include "flare/container/parallel_node_hash_map.h"
#include "flare/container/parallel_flat_hash_map.h"
#include "testing/gtest_wrap.h"

TEST(CaseIgnoredFlatHashMap, all) {
    flare::case_ignored_flat_hash_map<std::string, std::string> map;
    map["abc"] = "flare";
    EXPECT_EQ(map["Abc"], "flare");
    EXPECT_EQ(map["ABc"], "flare");
    EXPECT_EQ(map["AbC"], "flare");
    EXPECT_EQ(map["ABC"], "flare");
    EXPECT_EQ(map["abc"], "flare");
}