#ifndef RC_SIMPLE_LOGGER_LIB_SIMPLE_LOGGER_H
#define RC_SIMPLE_LOGGER_LIB_SIMPLE_LOGGER_H

#include <string_view>
#include <string>
#include <source_location>
#include <memory>
#include <format>
#include <optional>
#include <iostream>
#include <fstream>
#include <type_traits>

namespace SimpleLogger {
    void name_current_thread(std::string name);

    struct ConsumerInterface {
        static std::string_view current_thread_name();
        static std::string &thread_local_string_buf();

    protected:
        virtual ~ConsumerInterface() = default;

    public:
        static ConsumerInterface &current();

        virtual void write(std::string_view txt) = 0;

        template <typename ... ARGS> static void write_message(ConsumerInterface &ci, std::string_view type, std::source_location sl, std::format_string<ARGS...> fmt, ARGS && ... args) {
            auto thr = current_thread_name();
            auto &buf = thread_local_string_buf();
            buf.clear();
            std::format_to(std::back_inserter(buf), "{} {} {}:{} ", type, current_thread_name(), sl.file_name(), sl.line());
            std::vformat_to(std::back_inserter(buf), fmt.get(), std::make_format_args(args...));
            buf.push_back('\n');
            ci.write(buf);
        }
    };
    void initialize(std::shared_ptr<ConsumerInterface> ci);

    struct SourceLocation {
        std::source_location sl;

        SourceLocation(std::source_location sl = std::source_location::current()) : sl(sl) {}
    };
    
    template <typename ... ARGS> void error(SourceLocation sl, std::format_string<ARGS...> fmt, ARGS && ... args) {
        ConsumerInterface::write_message(ConsumerInterface::current(), "ERR", sl.sl, std::move(fmt), std::forward<ARGS>(args)...);
    }
    template <typename ... ARGS> void info(SourceLocation sl, std::format_string<ARGS...> fmt, ARGS && ... args) {
        ConsumerInterface::write_message(ConsumerInterface::current(), "inf", sl.sl, std::move(fmt), std::forward<ARGS>(args)...);
    }
    template <typename ... ARGS> void debug(SourceLocation sl, std::format_string<ARGS...> fmt, ARGS && ... args) {
        ConsumerInterface::write_message(ConsumerInterface::current(), "dbg", sl.sl, std::move(fmt), std::forward<ARGS>(args)...);
    }
    
    class FileOutputConsumer : public ConsumerInterface {
        std::ostream &output;
    public:
        FileOutputConsumer(std::ostream &output) : output(output) {}

        void write(std::string_view txt) {
            output.write(txt.data(), txt.size());
            output.flush();
        }
    };

    class TwoFileOutputConsumer : public ConsumerInterface {
        std::ostream &output1;
        std::ostream &output2;
    public:
        TwoFileOutputConsumer(std::ostream &output1, std::ostream &output2) : output1(output1), output2(output2) {}

        void write(std::string_view txt) override {
            output1.write(txt.data(), txt.size());
            output1.flush();
            output2.write(txt.data(), txt.size());
            output2.flush();
        }
    };

    template <typename STORAGE, typename BASE> class FileOutputConsumerWithOwnership : public BASE {
        std::unique_ptr<STORAGE> storage;
    public:
        template <typename ... ARGS> FileOutputConsumerWithOwnership(std::unique_ptr<STORAGE> storage, ARGS && ... args) : BASE(std::forward<ARGS>(args)...), storage(std::move(storage)) {}
    };
}

#define SIMPLE_LOGGER_INFO(...) SimpleLogger::info(std::source_location::current(), __VA_ARGS__)
#define SIMPLE_LOGGER_ERROR(...) SimpleLogger::error(std::source_location::current(), __VA_ARGS__)
#define SIMPLE_LOGGER_DEBUG(...) SimpleLogger::debug(std::source_location::current(), __VA_ARGS__)

#endif