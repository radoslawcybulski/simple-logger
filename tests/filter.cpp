#include <gtest/gtest.h>
#include "simple_logger/grep.h"
#include "simple_logger/simple_logger.h"

class FilterTest : public testing::Test {
};

static std::vector<std::string> filter_lines(const std::vector<std::string> &lines, std::vector<std::uint32_t> block_ids) {
    SimpleLogger::Filter filter;

    for(auto i = 0u; i < lines.size(); ++i) {
        filter.initialize_with_line(lines[i], i + 1);
    }
    filter.set_block_ids(block_ids);

    std::vector<std::string> result;
    for(size_t i = 0; i < lines.size(); ++i) {
        if (!filter.filter(lines[i], i + 1)) {
            result.push_back(std::string{lines[i]});
        }
    }
    return result;
}

TEST_F(FilterTest, simple_0) {
    std::vector<std::string> lines = {
        "0 000"
        "0 001"
        "0 002",
    };

    ASSERT_EQ(filter_lines(lines, { 0 }), (std::vector<std::string>{
        "0 000"
        "0 001"
        "0 002",
    }));
}

TEST_F(FilterTest, simple_1) {
    std::vector<std::string> lines = {
        "0 000",
        "1 110",
        "##!# 0->1 01",
        "0 001",
        "1 111",
        "2 220",
        "##!# 1->2 12",
        "2 221",
        "0 002",
    };

    ASSERT_EQ(filter_lines(lines, { 0 }), (std::vector<std::string>{
        "0 000",
        "1 110",
        "##!# 0->1 01",
        "0 001",
        "1 111",
        "2 220",
        "##!# 1->2 12",
        "2 221",
        "0 002",
    }));
}

TEST_F(FilterTest, simple_2) {
    std::vector<std::string> lines = {
        "0 000",
        "1 110",
        "##!# 0->1 01",
        "0 001",
        "1 111",
        "2 220",
        "##!# 1->2 12",
        "2 221",
        "0 002",
    };

    ASSERT_EQ(filter_lines(lines, { 1 }), (std::vector<std::string>{
        "1 110",
        "##!# 0->1 01",
        "1 111",
        "2 220",
        "##!# 1->2 12",
        "2 221",
    }));
}

TEST_F(FilterTest, simple_3) {
    std::vector<std::string> lines = {
        "0 000",
        "1 110",
        "##!# 0->1 01",
        "0 001",
        "1 111",
        "2 220",
        "##!# 1->2 12",
        "2 221",
        "0 002",
    };

    ASSERT_EQ(filter_lines(lines, { 2 }), (std::vector<std::string>{
        "##!# 0->1 01",
        "2 220",
        "##!# 1->2 12",
        "2 221",
    }));
}

TEST_F(FilterTest, simple_4) {
    std::vector<std::string> lines = {
        "0 000",
        "1 110",
        "##!# 0->1 01",
        "##!# 0->3 03",
        "0 001",
        "1 111",
        "2 220",
        "3 330",
        "##!# 1->2 12",
        "2 221",
        "0 002",
    };

    ASSERT_EQ(filter_lines(lines, { 2 }), (std::vector<std::string>{
        "##!# 0->1 01",
        "2 220",
        "##!# 1->2 12",
        "2 221",
    }));
}