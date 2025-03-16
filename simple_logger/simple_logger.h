#ifndef RC_SIMPLE_LOGGER_LIB_SIMPLE_LOGGER_H
#define RC_SIMPLE_LOGGER_LIB_SIMPLE_LOGGER_H

// #include <string_view>
// #include <string>
// #include <source_location>
// #include <memory>
// #include <format>
// #include <optional>
// #include <iostream>
// #include <fstream>
// #include <type_traits>
// #include <cstdint>
// #include <array>
// #include <functional>
// #include <span>
// #include <atomic>
// #include <chrono>

#include <source_location>
#include <string_view>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <iterator>

namespace SimpleLogger {
    enum class Level : std::uint8_t {
        Trace = 40, Debug = 80, Info = 120, Warning = 160, Error = 200, None = 255
    };

    void set_current_thread_name(std::string name);
    std::string_view current_thread_name();
    
    namespace Impl {
        void reset_everything();

        struct LoggingBlockInterface {
            virtual ~LoggingBlockInterface() = default;
        };
        struct OutputBufferInterface {
            virtual ~OutputBufferInterface() = default;

            virtual std::span<char> new_data() = 0;
            virtual std::span<char> more_data(char *) = 0;
            virtual void mark_end_of_header(char *) = 0;
            virtual std::span<char> rewind_to_header() = 0;
            virtual void emit(char *end_ptr, std::array<Level, 4> levels, Level level) = 0;
        };
        struct OutputInterface {
            virtual ~OutputInterface() = default;

            virtual OutputBufferInterface *output_buffer() = 0;
        };
        struct EmitInterface {
            virtual ~EmitInterface() = default;

            virtual void write(std::span<std::string_view>) = 0;
        };
        OutputInterface &output_interface();
        struct CharOutputIterator {
            Impl::OutputBufferInterface *buffer;
            char *ptr, *end;
            std::array<Level, 4> levels;
            Level level;

            CharOutputIterator(Impl::OutputBufferInterface *buffer, char *ptr, char *end, std::array<Level, 4> levels, Level level) : buffer(buffer), ptr(ptr), end(end), levels(levels), level(level) {}

            using difference_type = std::ptrdiff_t;

            CharOutputIterator& operator*() { return *this; }
            
            void operator=(char c) {
                if (c == '\n') {
                    buffer->emit(ptr, levels, level);
                    auto sp = buffer->rewind_to_header();
                    // we expect ++ after assignment
                    ptr = sp.data() - 1;
                    end = ptr + sp.size();
                }
                else {
                    *ptr = c;
                }
            }

            void increment() {
                ++ptr;
                if (ptr == end) {
                    auto new_span = buffer->more_data(ptr);
                    ptr = new_span.data();
                    end = ptr + new_span.size();
                }
            }
            CharOutputIterator& operator++() {
                increment();
                return *this;
            }

            CharOutputIterator operator++(int) {
                CharOutputIterator tmp = *this;
                increment();
                return tmp;
            }

            void mark_end_of_header() {
                buffer->mark_end_of_header(ptr);
            }
            void emit() {
                buffer->emit(ptr, levels, level);
            }
        };
        static_assert(std::output_iterator<CharOutputIterator, char>);
    }

    struct FormatWithSourceLocation {
        std::source_location sl;
        const char *format;

        FormatWithSourceLocation(const char *format, std::source_location sl = std::source_location::current()) : sl(sl), format(format) {}
    };

    class Logger {
    public:
        struct Data {
            std::string name;
            std::unordered_map<std::string, std::unique_ptr<Data>> children;
            std::array<Level, 4> levels;

            Data(std::string name) : name(std::move(name)) {
                for(auto &q : levels) q = Level::None;
            }
        };
    
    private:
        Data *data;

        Logger(Data *data) : data(data) {}

        std::optional<std::array<Level, 4>> should_log(Level) const;
        void emit_token_links();
        std::uint64_t current_token_index() const;


    public:
        virtual ~Logger() = default;

        std::string_view name() const;
        
        template <typename ... ARGS> void log(const FormatWithSourceLocation &format, Level level, std::string_view level_name, ARGS && ... args) {
            auto levels = should_log(level);
            if (!levels) return;
            emit_token_links();
            auto output_buffer = Impl::output_interface().output_buffer();

            auto data = output_buffer->new_data();
            auto it = Impl::CharOutputIterator(output_buffer, data.data(), data.data() + data.size(), *levels, level);
            it = std::format_to(it, "{} {:%F %T} {}", current_token_index(), std::chrono::system_clock::now(), level_name);
            auto thr = current_thread_name();
            if (!thr.empty()) {
                it = std::format_to(it, " [{}]", thr);
            }
            it = std::format_to(it, " {}:{}", format.sl.file_name(), format.sl.line());
            auto logger_name = name();
            if (!logger_name.empty()) {
                it = std::format_to(it, " {}", logger_name);
            }
            it = std::format_to(it, ": ");
            it.mark_end_of_header();

            it = std::vformat_to(it, format.format, std::make_format_args(std::forward<const ARGS>(args)...));
            it.emit();
        }
        template <typename ... ARGS> void error(const FormatWithSourceLocation &format,  ARGS && ... args) { log(format, Level::Error, "ERROR", std::forward<ARGS>(args)...); }
        template <typename ... ARGS> void warning(const FormatWithSourceLocation &format,  ARGS && ... args) { log(format, Level::Warning, "WARNING", std::forward<ARGS>(args)...); }
        template <typename ... ARGS> void info(const FormatWithSourceLocation &format,  ARGS && ... args) { log(format, Level::Info, "INFO", std::forward<ARGS>(args)...); }
        template <typename ... ARGS> void debug(const FormatWithSourceLocation &format,  ARGS && ... args) { log(format, Level::Debug, "DEBUG", std::forward<ARGS>(args)...); }
        template <typename ... ARGS> void trace(const FormatWithSourceLocation &format,  ARGS && ... args) { log(format, Level::Trace, "TRACE", std::forward<ARGS>(args)...); }

        static Logger logger(std::string_view name);
        static Logger root();
    };

    inline Logger logger(std::string_view name) { return Logger::logger(name); }

    class LoggingBlock {
        class LoggingBlockImpl;
        std::unique_ptr<LoggingBlockImpl> block;
    public:
        enum class EmitEntering {
            Always,
            OnFirstMessage,
        };
        enum class EmitLeaving {
            Always,
            AfterFirstMessage,
            Never,
        };
        LoggingBlock(std::string description, std::optional<Level> overwrite_log_level, EmitEntering emit_entering = EmitEntering::OnFirstMessage, EmitLeaving emit_leaving = EmitLeaving::Never);
        LoggingBlock(std::string description, EmitEntering emit_entering = EmitEntering::OnFirstMessage, EmitLeaving emit_leaving = EmitLeaving::Never) :
            LoggingBlock(std::move(description), std::nullopt, emit_entering, emit_leaving) {}
        LoggingBlock(LoggingBlock &&) = delete;
        ~LoggingBlock();

        LoggingBlock &operator = (LoggingBlock &&) = delete;

        void emit();
    };

    inline Logger root() { return Logger::root(); }

    struct Configuration {
        Level default_level = Level::Info;
        std::unordered_map<std::string, Level> logger_levels;
    };

    void add_output(std::unique_ptr<Impl::EmitInterface>, Configuration config = {});
    void add_console_output();

    template <typename ... ARGS> void error(const FormatWithSourceLocation &format,  ARGS && ... args) { root().error(format, std::forward<ARGS>(args)...); }
    template <typename ... ARGS> void warning(const FormatWithSourceLocation &format,  ARGS && ... args) { root().warning(format, std::forward<ARGS>(args)...); }
    template <typename ... ARGS> void info(const FormatWithSourceLocation &format,  ARGS && ... args) { root().info(format, std::forward<ARGS>(args)...); }
    template <typename ... ARGS> void debug(const FormatWithSourceLocation &format,  ARGS && ... args) { root().debug(format, std::forward<ARGS>(args)...); }
    template <typename ... ARGS> void trace(const FormatWithSourceLocation &format,  ARGS && ... args) { root().trace(format, std::forward<ARGS>(args)...); }
}

#endif