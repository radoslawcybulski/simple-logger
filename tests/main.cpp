#include <gtest/gtest.h>
#include "simple_logger/simple_logger.h"
#include <thread>
#include <ranges>
#include <source_location>

class SimpleTest : public testing::Test {
protected:
    std::vector<std::string> lines;
    std::vector<std::string> lines2;

    struct Emit : public SimpleLogger::Impl::EmitInterface {
        std::vector<std::string> &lines;

        Emit(std::vector<std::string> &lines) : lines(lines) {
        }

        void write(std::span<std::string_view> data) override {
            lines.push_back("");
            size_t s = 0;
            for(auto &d : data) {
                s += d.size();
            }
            lines.back().reserve(s);
            for(auto &d : data) {
                lines.back() += d;
            }
            lines.back().resize(lines.back().size() - 1); // strip ending newline
        }
    };
    static void validate_size(std::string_view line, size_t &pos, size_t size, std::string_view name) {
        if (line.size() < pos + size) {
            FAIL() << "invalid line `" << line << "` - not enough characters, expected " << size << ", got " << (pos + size - line.size()) << ", when parsing " << name << " at position " << pos;
        }
    }
    static void check_char(std::string_view line, size_t &pos, char c, std::string_view name) {
        validate_size(line, pos, 1, name);
        auto cc = line[pos++];
        if (cc != c) {
            FAIL() << "invalid line `" << line << "` - invalid character, expected " << c << ", got " << cc << " when parsing " << name << " at position " << (pos - 1);
        }
    }
    static void check_digits(std::string_view line, size_t &pos, size_t size, std::string_view name) {
        validate_size(line, pos, size, name);
        while(pos < line.size() && size > 0) {
            auto c = line[pos++];
            if (c < '0' || c > '9') {
                FAIL() << "invalid line `" << line << "` - not a digit, when parsing " << name << " at position " << (pos - 1);
            }
            --size;
        }
    }
    static void check_digits(std::string_view line, size_t &pos, size_t min_size, size_t max_size, std::string_view name) {
        auto end = pos + max_size;
        check_digits(line, pos, min_size, name);
        while(pos < end && pos < line.size()) {
            auto c = line[pos];
            if (c < '0' || c > '9') break;
            ++pos;
        }
    }
    static void check_text(std::string_view line, size_t &pos, const std::vector<std::string_view> &possibilities, std::string_view *result = nullptr) {
        for(auto txt : possibilities) {
            if (line.substr(pos).starts_with(txt)) {
                pos += txt.size();
                if (result) *result = txt;
                return;
            }
        }
        std::string tmp;
        for(auto p : possibilities) {
            if (!tmp.empty()) tmp += " ";
            tmp += p;
        }
        FAIL() << "invalid line `" << line << "` - expected one of " << tmp << " at position " << pos;
        return;
    }
    void prepare_skip_datetime(std::string_view line, size_t &pos) {
        check_digits(line, pos, 4, "datetime");
        check_char(line, pos, '-', "datetime");
        check_digits(line, pos, 2, "datetime");
        check_char(line, pos, '-', "datetime");
        check_digits(line, pos, 2, "datetime");
        
        check_char(line, pos, ' ', "datetime");

        check_digits(line, pos, 2, "datetime");
        check_char(line, pos, ':', "datetime");
        check_digits(line, pos, 2, "datetime");
        check_char(line, pos, ':', "datetime");
        check_digits(line, pos, 2, "datetime");
        check_char(line, pos, '.', "datetime");
        check_digits(line, pos, 6, 9, "datetime");
    }

    void prepare(size_t min, std::vector<std::string> *timestamps = nullptr, std::source_location sl = std::source_location::current()) {
        auto max = sl.line();
        if (timestamps) timestamps->clear();
        for(auto &llines : { std::ref(lines), std::ref(lines2) }) {
            for(auto &line : llines.get()) {
                size_t pos = 0;
                if (line.starts_with("##!# ")) {
                    pos = 5;

                    auto start = pos;
                    while(pos < line.size() && line[pos] != ' ') {
                        ++pos;
                    }
                    auto key = std::string{ line.substr(start, pos - start) };
                    check_char(line, pos, ' ', "token key");

                    start = pos;
                    prepare_skip_datetime(line, pos);
                    if (timestamps) {
                        timestamps->push_back(std::string{line.substr(start, pos - start)});
                    }
                    check_char(line, pos, ' ', "datetime");
                    if (pos < line.size() && line[pos] == '[') {
                        check_char(line, pos, '[', "thread name");
                        check_text(line, pos, { "test_thread" });
                        check_char(line, pos, ']', "thread name");
                        check_char(line, pos, ' ', "thread name");
                    }
                    line = key + " " + line.substr(pos);
                    continue;
                }
                while(pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
                    ++pos;
                }
                auto key = std::string{ line.substr(0, pos) };
                check_char(line, pos, ' ', "token key");
                auto start = pos;
                prepare_skip_datetime(line, pos);
                if (timestamps) {
                    timestamps->push_back(std::string{line.substr(start, pos - start)});
                }
                check_char(line, pos, ' ', "datetime");

                std::string_view level;
                check_text(line, pos, { "ERROR", "INFO", "WARNING", "DEBUG", "TRACE" }, &level);

                // 2026-02-15 07:41:15.319150845 ERROR [test_thread] /home/y/work/simple-logger/tests/main.cpp:48: q 4 w 5
                check_char(line, pos, ' ', "thread name");
                check_char(line, pos, '[', "thread name");
                check_text(line, pos, { "test_thread" });
                check_char(line, pos, ']', "thread name");
                check_char(line, pos, ' ', "thread name");

                auto p = line.find(":", pos);
                ASSERT_NE(p, std::string_view::npos);
                auto file = line.substr(pos, p - pos);
                if (file != sl.file_name()) {
                    FAIL() << "invalid line `" << line << "` - expected file name `" << sl.file_name() << "`, got `" << file << "` at position " << pos;
                }
                pos = p + 1;
                size_t val = 0;
                while(p < line.size() && line[pos] >= '0' && line[pos] <= '9') {
                    val = val * 10 + line[pos] - '0';
                    ++pos;
                }
                if (val < min || val > max) {
                    FAIL() << "invalid line `" << line << "` - expected file line between " << min << " and " << max << ", got " << val << " at position " << pos;
                }
                std::string logger_name;
                if (line[pos] != ':') {
                    check_char(line, pos, ' ', "logger name");
                    auto start = pos;
                    while(pos < line.size() && line[pos] != ':') {
                        ++pos;
                    }
                    logger_name += line.substr(start, pos - start);
                    logger_name += " ";
                }
                check_char(line, pos, ':', "position");
                check_char(line, pos, ' ', "position");
                line = key + " " + std::string{ level } + " " + logger_name + line.substr(pos);
            }
            timestamps = nullptr;
        }
    }
	void SetUp() override {
        SimpleLogger::Impl::reset_everything();
        SimpleLogger::add_output(std::make_unique<Emit>(lines), { SimpleLogger::Level::Debug,
        {
            { "C", SimpleLogger::Level::Debug },
            { "C/D", SimpleLogger::Level::Warning },
            { "C/D/E", SimpleLogger::Level::Debug },
            { "F", SimpleLogger::Level::Debug },
        } });
        SimpleLogger::set_current_thread_name("test_thread");
    }
	void TearDown() override {
	}
};

TEST_F(SimpleTest, empty) {
}

TEST_F(SimpleTest, simple) {
    const auto min_line = __LINE__;
    SimpleLogger::error("q {} w {}", 1, 2);
    SimpleLogger::warning("q {} w {}", 3, 4);
    SimpleLogger::info("q {} w {}", 5, 6);
    SimpleLogger::debug("q {} w {}", 7, 8);
    SimpleLogger::trace("q {} w {}", 9, 10);
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR q 1 w 2",
        "0 WARNING q 3 w 4",
        "0 INFO q 5 w 6",
        "0 DEBUG q 7 w 8"
    }));
}

TEST_F(SimpleTest, big_line) {
    std::string text;
    for(auto i = 0u; i < 1000; ++i) {
        text += "0123456789abcdef";
    }
    const auto min_line = __LINE__;
    SimpleLogger::error("{}", text);
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR " + text
    }));
}

TEST_F(SimpleTest, multi_line) {

    std::vector<std::string> expected_lines;
    std::string text;
    
    text.reserve(1001 * 16 + 500);
    for(auto i = 0u; i < 1000; ++i) {
        if (i > 0) text += "\n";
        text += "0123456789abcdef";
        expected_lines.push_back("0 ERROR 0123456789abcdef");
    }
    const auto min_line = __LINE__;
    SimpleLogger::error("{}", text);
    std::vector<std::string> timestamps;
    prepare(min_line, &timestamps);

    ASSERT_EQ(timestamps.size(), 1000);
    for(auto i = 0u; i < timestamps.size(); ++i) {
        ASSERT_EQ(timestamps[i], timestamps[0]);
    }
    ASSERT_EQ(lines, expected_lines);
}

TEST_F(SimpleTest, with_block) {
    const auto min_line = __LINE__;
    SimpleLogger::error("q {} w {}", 1, 2);
    {
        SimpleLogger::LoggingBlock lb{ "qwerty" };
        SimpleLogger::warning("q {} w {}", 3, 4);
        SimpleLogger::trace("q {} w {}", 9, 10);
    }
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR q 1 w 2",
        "0->1 qwerty",
        "1 WARNING q 3 w 4",
    }));
}

TEST_F(SimpleTest, with_logger) {
    const auto min_line = __LINE__;
    auto a = SimpleLogger::logger("A");
    auto b = SimpleLogger::logger("A/B");
    SimpleLogger::error("q {} w {}", 1, 2);
    a.error("q {} w {}", 3, 4);
    b.error("q {} w {}", 5, 6);
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR q 1 w 2",
        "0 ERROR A q 3 w 4",
        "0 ERROR A/B q 5 w 6",
    }));
}

TEST_F(SimpleTest, with_logger_and_config) {
    const auto min_line = __LINE__;
    auto c = SimpleLogger::logger("C");
    auto d = SimpleLogger::logger("C/D");
    auto e = SimpleLogger::logger("C/D/E");
    auto f = SimpleLogger::logger("F");
    c.debug("q {} w {}", 1, 2);
    c.trace("q {} w {}", 3, 4);
    d.debug("q {} w {}", 5, 6);
    d.info("q {} w {}", 7, 8);
    d.warning("q {} w {}", 9, 10);
    e.debug("q {} w {}", 11, 12);
    e.trace("q {} w {}", 13, 14);
    f.debug("q {} w {}", 15, 16);
    f.trace("q {} w {}", 17, 18);

    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 DEBUG C q 1 w 2",
        "0 WARNING C/D q 9 w 10",
        "0 DEBUG C/D/E q 11 w 12",
        "0 DEBUG F q 15 w 16",
    }));
}

TEST_F(SimpleTest, with_multiple_outputs) {
    SimpleLogger::add_output(std::make_unique<Emit>(lines2), { SimpleLogger::Level::Debug,
    {
        { "C", SimpleLogger::Level::Warning },
        { "C/D", SimpleLogger::Level::Debug },
        { "C/D/E", SimpleLogger::Level::Warning },
        { "F", SimpleLogger::Level::Debug },
    } });

    const auto min_line = __LINE__;
    auto c = SimpleLogger::logger("C");
    auto d = SimpleLogger::logger("C/D");
    auto e = SimpleLogger::logger("C/D/E");
    auto f = SimpleLogger::logger("F");
    c.debug("q {} w {}", 1, 2);
    c.warning("q {} w {}", 3, 4);
    d.debug("q {} w {}", 5, 6);
    d.warning("q {} w {}", 7, 8);
    e.debug("q {} w {}", 9, 10);
    e.warning("q {} w {}", 11, 12);
    f.debug("q {} w {}", 13, 14);

    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 DEBUG C q 1 w 2",
        "0 WARNING C q 3 w 4",
        "0 WARNING C/D q 7 w 8",
        "0 DEBUG C/D/E q 9 w 10",
        "0 WARNING C/D/E q 11 w 12",
        "0 DEBUG F q 13 w 14",
    }));
    ASSERT_EQ(lines2, (std::vector<std::string>{
        "0 WARNING C q 3 w 4",
        "0 DEBUG C/D q 5 w 6",
        "0 WARNING C/D q 7 w 8",
        "0 WARNING C/D/E q 11 w 12",
        "0 DEBUG F q 13 w 14",
    }));
}

TEST_F(SimpleTest, with_multi_block) {
    const auto min_line = __LINE__;
    SimpleLogger::error("q {} w {}", 1, 2);
    {
        SimpleLogger::LoggingBlock lb{ "qwerty1" };
        SimpleLogger::warning("q {} w {}", 3, 4);
        SimpleLogger::trace("q {} w {}", 9, 10);
        {
            SimpleLogger::LoggingBlock lb{ "qwerty2" };
            SimpleLogger::warning("q {} w {}", 5, 6);
            SimpleLogger::trace("q {} w {}", 11, 12);
            {
                SimpleLogger::LoggingBlock lb{ "qwerty3" };
                SimpleLogger::warning("q {} w {}", 7, 8);
                SimpleLogger::trace("q {} w {}", 13, 14);
            }
        }
    }
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR q 1 w 2",
        "0->1 qwerty1",
        "1 WARNING q 3 w 4",
        "1->2 qwerty2",
        "2 WARNING q 5 w 6",
        "2->3 qwerty3",
        "3 WARNING q 7 w 8",
    }));
}

TEST_F(SimpleTest, with_block_and_level_overwrite) {
    const auto min_line = __LINE__;
    SimpleLogger::error("q {} w {}", 1, 2);
    {
        SimpleLogger::LoggingBlock lb{ "qwerty", SimpleLogger::Level::Trace };
        SimpleLogger::warning("q {} w {}", 3, 4);
        SimpleLogger::trace("q {} w {}", 9, 10);
    }
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR q 1 w 2",
        "0->1 qwerty",
        "1 WARNING q 3 w 4",
        "1 TRACE q 9 w 10",
    }));
}

TEST_F(SimpleTest, with_block_and_no_log) {
    const auto min_line = __LINE__;
    SimpleLogger::error("q {} w {}", 1, 2);
    {
        SimpleLogger::LoggingBlock lb{ "qwerty", SimpleLogger::Level::Trace };
    }
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR q 1 w 2",
    }));
}

TEST_F(SimpleTest, with_multi_block_and_level_overwrite) {
    const auto min_line = __LINE__;
    SimpleLogger::error("q {} w {}", 1, 2);
    {
        SimpleLogger::LoggingBlock lb{ "qwerty1", SimpleLogger::Level::Trace };
        SimpleLogger::warning("q {} w {}", 3, 4);
        SimpleLogger::trace("q {} w {}", 9, 10);
        {
            SimpleLogger::LoggingBlock lb{ "qwerty2" };
            SimpleLogger::warning("q {} w {}", 5, 6);
            SimpleLogger::trace("q {} w {}", 11, 12);
            {
                SimpleLogger::LoggingBlock lb{ "qwerty3", SimpleLogger::Level::Error };
                SimpleLogger::warning("q {} w {}", 7, 8);
                SimpleLogger::trace("q {} w {}", 13, 14);
                {
                    SimpleLogger::LoggingBlock lb{ "qwerty4", SimpleLogger::Level::Trace };
                    SimpleLogger::warning("q {} w {}", 9, -9);
                    SimpleLogger::trace("q {} w {}", 15, 16);
                }
            }
        }
    }
    prepare(min_line);
    ASSERT_EQ(lines, (std::vector<std::string>{
        "0 ERROR q 1 w 2",
        "0->1 qwerty1",
        "1 WARNING q 3 w 4",
        "1 TRACE q 9 w 10",
        "1->2 qwerty2",
        "2 WARNING q 5 w 6",
        "2 TRACE q 11 w 12",
        "3->4 qwerty4",
        "2->3 qwerty3",
        "4 WARNING q 9 w -9",
        "4 TRACE q 15 w 16",
    }));
}
