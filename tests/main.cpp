#include <gtest/gtest.h>
#include "simple_logger/simple_logger.h"
#include <thread>

class SimpleTest : public testing::Test {
	void SetUp() override {
        SimpleLogger::name_current_thread("test_thread");
    }
	void TearDown() override {
	}
};

TEST_F(SimpleTest, create1) {
    struct Files {
        std::ofstream file;
    };
    auto f = std::make_unique<Files>();
    auto &f1 = std::cout;
    auto &f2 = f->file;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<Files, SimpleLogger::TwoFileOutputConsumer>>(std::move(f), f1, f2);
}
TEST_F(SimpleTest, create2) {
    struct Files {
    };
    auto f = std::make_unique<Files>();
    auto &f1 = std::cout;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<Files, SimpleLogger::FileOutputConsumer>>(std::move(f), f1);
}

TEST_F(SimpleTest, empty1) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::FileOutputConsumer>>(nullptr, temp);
    SimpleLogger::initialize(ci);
    ASSERT_EQ(temp.str(), "");
}

TEST_F(SimpleTest, empty2) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::TwoFileOutputConsumer>>(nullptr, temp, temp);
    SimpleLogger::initialize(ci);
    ASSERT_EQ(temp.str(), "");
}

TEST_F(SimpleTest, multithreaded) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::FileOutputConsumer>>(nullptr, temp);
    SimpleLogger::initialize(ci);
    
    auto loc = std::source_location::current();
    SimpleLogger::info({}, "QWERTY {} {}", 1, 2);

    std::source_location loc2;
    auto thr = std::thread([&]() {
        SimpleLogger::name_current_thread("test_thread2");

        loc2 = std::source_location::current();
        SimpleLogger::error({}, "QWERTY {} {}", 3, 4);    
    });
    thr.join();

    ASSERT_EQ(temp.str(), std::format("inf test_thread {}:{} QWERTY 1 2\n", loc.file_name(), loc.line() + 1) + std::format("ERR test_thread2 {}:{} QWERTY 3 4\n", loc2.file_name(), loc2.line() + 1));
}

TEST_F(SimpleTest, info1) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::FileOutputConsumer>>(nullptr, temp);
    SimpleLogger::initialize(ci);
    
    auto loc = std::source_location::current();
    SIMPLE_LOGGER_INFO("QWERTY {} {}", 1, 2);

    ASSERT_EQ(temp.str(), std::format("inf test_thread {}:{} QWERTY 1 2\n", loc.file_name(), loc.line() + 1));
}
TEST_F(SimpleTest, error1) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::FileOutputConsumer>>(nullptr, temp);
    SimpleLogger::initialize(ci);
    
    auto loc = std::source_location::current();
    SIMPLE_LOGGER_ERROR("QWERTY {} {}", 1, 2);

    ASSERT_EQ(temp.str(), std::format("ERR test_thread {}:{} QWERTY 1 2\n", loc.file_name(), loc.line() + 1));
}
TEST_F(SimpleTest, debug1) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::FileOutputConsumer>>(nullptr, temp);
    SimpleLogger::initialize(ci);
    
    auto loc = std::source_location::current();
    SIMPLE_LOGGER_DEBUG("QWERTY {} {}", 1, 2);

    ASSERT_EQ(temp.str(), std::format("dbg test_thread {}:{} QWERTY 1 2\n", loc.file_name(), loc.line() + 1));
}

static std::string duplicate_string(std::string tmp) {
    tmp += tmp;
    return tmp;
}
TEST_F(SimpleTest, info2) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::TwoFileOutputConsumer>>(nullptr, temp, temp);
    SimpleLogger::initialize(ci);
    
    auto loc = std::source_location::current();
    SimpleLogger::info({}, "QWERTY {} {}", 1, 2);

    ASSERT_EQ(temp.str(), duplicate_string(std::format("inf test_thread {}:{} QWERTY 1 2\n", loc.file_name(), loc.line() + 1)));
}
TEST_F(SimpleTest, error2) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::TwoFileOutputConsumer>>(nullptr, temp, temp);
    SimpleLogger::initialize(ci);
    
    auto loc = std::source_location::current();
    SimpleLogger::error({}, "QWERTY {} {}", 1, 2);

    ASSERT_EQ(temp.str(), duplicate_string(std::format("ERR test_thread {}:{} QWERTY 1 2\n", loc.file_name(), loc.line() + 1)));
}
TEST_F(SimpleTest, debug2) {
    std::ostringstream temp;
    auto ci = std::make_shared<SimpleLogger::FileOutputConsumerWithOwnership<int, SimpleLogger::TwoFileOutputConsumer>>(nullptr, temp, temp);
    SimpleLogger::initialize(ci);
    
    auto loc = std::source_location::current();
    SimpleLogger::debug({}, "QWERTY {} {}", 1, 2);

    ASSERT_EQ(temp.str(), duplicate_string(std::format("dbg test_thread {}:{} QWERTY 1 2\n", loc.file_name(), loc.line() + 1)));
}




