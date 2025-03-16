#include "simple_logger.h"
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <list>
#include <format>
#include <array>
#include <functional>

namespace SimpleLogger {
    namespace {
        struct Data {
            std::atomic_int next_thread_id{ 1 };
            Level global_default_level = Level::Info;
            Logger::Data data_root{""};
            std::mutex data_mutex;
        };
        static Data &global_data() {
            static Data data;
            return data;
        }
    }
    static thread_local std::string thread_name = "thread-" + std::to_string(global_data().next_thread_id.fetch_add(1, std::memory_order_relaxed));

    void set_current_thread_name(std::string name) {
        thread_name = std::move(name);
    }
    std::string_view current_thread_name() {
        return thread_name;
    }

    namespace Impl {
        struct Token {
            std::string description;
            std::shared_ptr<Token> previous_token;
            std::uint64_t token_index;
            std::optional<Level> overwrite_log_level;
            std::atomic_bool emited{ false };
        };
        std::shared_ptr<Token> create_root_token() {
            auto t = std::make_shared<Token>();
            t->emited = true;
            return t;
        }
        static thread_local std::shared_ptr<Token> current_token = create_root_token();
        static std::atomic_uint64_t global_token_index{ 0 };

        Logger::Data *find_data(std::string_view name) {
            Logger::Data *cur = &global_data().data_root;

            if (!name.empty()) {
                size_t pos = 0;
                if (name[0] == '/') ++pos;

                while(pos < name.size()) {
                    auto x = name.find('/', pos);
                    if (x == pos) {
                        throw std::runtime_error("Invalid logger name: empty component");
                    }
                    if (x == std::string_view::npos) {
                        x = name.size();
                    }
                    auto sub = name.substr(pos, x - pos);
                    pos = x + 1;

                    auto it = cur->children.emplace(sub, nullptr);
                    if (it.second) {
                        it.first->second = std::make_unique<Logger::Data>(std::string{ name.substr(0, pos - 1) });
                        it.first->second->levels = global_data().data_root.levels;
                    }
                    cur = it.first->second.get();
                }
            }
            return cur;
        }
        class ConsoleEmit : public EmitInterface {
        public:
            ConsoleEmit() = default;

            void write(std::span<std::string_view> data) override {
                static std::mutex mutex;

                if (data.size() == 1) {
                    std::fwrite(data[0].data(), 1, data[0].size(), stderr);
                }
                else {
                    std::lock_guard<std::mutex> lock(mutex);
                    for(auto &d : data) {
                        std::fwrite(d.data(), 1, d.size(), stderr);
                    }
                }
                std::fflush(stderr);
            }
        };
        class OstreamEmit : public EmitInterface {
            std::ostream &out;

            OstreamEmit(std::ostream &out) : out(out) {}

            void write(std::span<std::string_view> data) override {
                static std::mutex mutex;

                if (data.size() == 1) {
                    out.write(data[0].data(), data[0].size());
                }
                else {
                    std::lock_guard<std::mutex> lock(mutex);
                    for(auto &d : data) {
                        out.write(d.data(), d.size());
                    }
                }
                out.flush();
            }
        };
        class FileEmit : public EmitInterface {
            std::FILE *file;
            
            FileEmit(std::FILE *file) : file(file) {}

            void write(std::span<std::string_view> data) override {
                static std::mutex mutex;

                if (data.size() == 1) {
                    std::fwrite(data[0].data(), 1, data[0].size(), file);
                }
                else {
                    std::lock_guard<std::mutex> lock(mutex);
                    for(auto &d : data) {
                        std::fwrite(d.data(), 1, d.size(), file);
                    }
                }
                std::fflush(file);
            }
        };
        class DefaultOutput : public OutputInterface {
            std::array<std::unique_ptr<EmitInterface>, 4> emit_interfaces;
            std::atomic<std::uint8_t> emit_interfaces_count{ 0 };
            std::mutex emit_mutex;

            class DefaultBuffer : public Impl::OutputBufferInterface {
                DefaultOutput &mi;

                std::vector<char> buffer;
                size_t end_of_header_used;
                size_t size_used;
                
                void resize_latest_entry(char *old_ptr) {
                    assert(buffer.data() <= old_ptr && buffer.data() + buffer.size() >= old_ptr);
                    size_used = old_ptr - buffer.data();
                }
            public:
                DefaultBuffer(DefaultOutput &mi) : mi(mi) {
                    buffer.resize(1024 * 8);
                    size_used = 0;
                    end_of_header_used = 0;
                }
                void mark_end_of_header(char *ptr) override {
                    assert(buffer.data() <= ptr && buffer.data() + buffer.size() >= ptr);
                    size_used = end_of_header_used = ptr - buffer.data();
                }
                std::span<char> rewind_to_header() override {
                    return { buffer.data() + end_of_header_used, buffer.size() - end_of_header_used };
                }

                std::span<char> new_data() override {
                    size_used = end_of_header_used = 0;
                    return { buffer.data(), buffer.size() };
                }
                std::span<char> more_data(char *old_ptr) override {
                    assert(old_ptr);
                    assert(buffer.data() <= old_ptr && buffer.data() + buffer.size() >= old_ptr);
                    size_used = old_ptr - buffer.data();
                    if (size_used == buffer.size()) {
                        buffer.resize(buffer.size() * 2);
                    }
                    return { buffer.data() + size_used, buffer.size() - size_used };
                }

                void emit(char *end_ptr, std::array<Level, 4> levels, Level level) override {
                    auto p = more_data(end_ptr);
                    p[0] = '\n';
                    more_data(p.data() + 1);

                    auto mx = std::min(mi.emit_interfaces_count.load(std::memory_order_relaxed), (std::uint8_t)4);
                    std::string_view buffer_view = { buffer.data(), size_used };
                    std::array<std::string_view, 1> buffer_data = { buffer_view };
                    std::span<std::string_view> data = buffer_data;
                    for(auto i = 0u; i < mx; ++i) {
                        if (levels[i] > level) continue;
                        mi.emit_interfaces[i]->write(data);
                    }
                }
            };
        public:
            void add_emit_interface(std::unique_ptr<EmitInterface> ei, Configuration config) {
                std::lock_guard<std::mutex> lock(emit_mutex);
                auto index = emit_interfaces_count++;
                if (index + 1 >= emit_interfaces.size()) {
                    --emit_interfaces_count;
                    throw std::runtime_error("No more emit interfaces can be added to default output");
                }
                emit_interfaces[index] = std::move(ei);

                std::lock_guard lock2(global_data().data_mutex);
                global_data().data_root.levels[index] = config.default_level;
                for(auto &it : config.logger_levels) {
                    auto data = find_data(it.first);
                    data->levels[index] = it.second;
                }
                std::function<void(Logger::Data*)> update_level = [&](Logger::Data *data) {
                    for(auto i = 0u; i <= index; ++i) {
                        if (data->levels[i] == Level::None) {
                            data->levels[i] = global_data().data_root.levels[i];
                        }
                    }
                    for(auto &child : data->children) {
                        update_level(child.second.get());
                    }
                };
            }
            void reset() {
                std::lock_guard<std::mutex> lock(emit_mutex);
                std::lock_guard lock2(global_data().data_mutex);
                global_data().data_root = Logger::Data{""};
                global_data().next_thread_id = 1;
                emit_interfaces_count = 0;
                for(auto &q : emit_interfaces) q.reset();
                current_token = create_root_token();
                global_token_index = 0;
            }
            OutputBufferInterface *output_buffer() override {
                static thread_local DefaultBuffer df{ *this };
                return &df;
            }
        };
        static DefaultOutput default_output_value;

        void add_output(std::unique_ptr<Impl::EmitInterface> ei, Configuration config) {
            default_output_value.add_emit_interface(std::move(ei), std::move(config));
        }
        OutputInterface &output_interface() {
            return default_output_value;
        }
        void reset_everything() {
            default_output_value.reset();
        }
            // auto it = Impl::CharOutputIterator(output_buffer, data.data(), data.data() + data.size(), *levels, level);
            // it = std::format_to(it, "{} {:%F %T} {}", current_token_index(), std::chrono::system_clock::now(), level_name);
            // auto thr = current_thread_name();

        void emit_token_links_message(Token &token, bool leaving) {
            auto output_buffer = Impl::output_interface().output_buffer();
            auto data = output_buffer->new_data();
            auto it = Impl::CharOutputIterator(output_buffer, data.data(), data.data() + data.size(), std::array<Level, 4>{ Level::Info, Level::Info, Level::Info, Level::Info }, Level::Info);
            if (!leaving) {
                it = std::format_to(it, "##!# {}->{} {:%F %T}", token.previous_token->token_index, token.token_index, std::chrono::system_clock::now());
            }
            else {
                it = std::format_to(it, "##!# {} {:%F %T}", token.token_index, std::chrono::system_clock::now());
            }
            auto thr = current_thread_name();
            if (!thr.empty()) {
                it = std::format_to(it, " [{}]", thr);
            }
            it = std::format_to(it, " {}", token.description);
            if (leaving) {
                it = std::format_to(it, " (done)");
            }
            it.emit();
        }
        void emit_token_links(Token &token) {
            if (!token.emited) {
                std::array<Level, 4> all_info_levels = { Level::Info, Level::Info, Level::Info, Level::Info };

                auto p = &token;
                auto output_buffer = Impl::output_interface().output_buffer();
                while(p) {
                    if (p->emited.exchange(true)) break;
                    emit_token_links_message(*p, false);
                    p = p->previous_token.get();
                }
            }        
        }
    }

    void add_output(std::unique_ptr<Impl::EmitInterface> ei, Configuration config) {
        Impl::add_output(std::move(ei), std::move(config));
    }
    void add_console_output() {
        Impl::add_output(std::make_unique<Impl::ConsoleEmit>(), {});
    }
    Logger Logger::logger(std::string_view name) {
        std::lock_guard lock(global_data().data_mutex);
        return Logger{ Impl::find_data(name) };
    }
    Logger Logger::root() {
        return Logger{ &global_data().data_root };
    }
    std::optional<std::array<Level, 4>> Logger::should_log(Level level) const {
        if (Impl::current_token->overwrite_log_level) {
            if (level < *Impl::current_token->overwrite_log_level) return std::nullopt;
            return std::array<Level, 4>{ level, level, level, level };
        }
        for(auto i = 0u; i < data->levels.size(); ++i) {
            auto lev = data->levels[i];
            if (level >= lev) return data->levels;
        }
        return std::nullopt;
    }
    void Logger::emit_token_links() {
        Impl::emit_token_links(*Impl::current_token);
    }
    std::uint64_t Logger::current_token_index() const {
        return Impl::current_token->token_index;
    }
    std::string_view Logger::name() const {
        return data->name;
    }
    class LoggingBlock::LoggingBlockImpl  {
        std::shared_ptr<Impl::Token> token;
        EmitLeaving emit_leaving;
    public:

        LoggingBlockImpl(std::string description, std::optional<Level> overwrite_log_level, EmitLeaving emit_leaving) {
            auto t = std::make_shared<Impl::Token>();
            t->description = std::move(description);
            t->previous_token = std::move(Impl::current_token);
            t->token_index = ++Impl::global_token_index;
            t->overwrite_log_level = overwrite_log_level ? overwrite_log_level : t->previous_token->overwrite_log_level;
            Impl::current_token = t;
            this->token = std::move(t);
            this->emit_leaving = emit_leaving;
        }
        ~LoggingBlockImpl() {
            Impl::current_token = Impl::current_token->previous_token;
        }
        void emit() {
            Impl::emit_token_links(*token);
        }
        void emit_leaving_if_needed() {
            if (emit_leaving == EmitLeaving::Always || (emit_leaving == EmitLeaving::AfterFirstMessage && token->emited.load())) {
                Impl::emit_token_links_message(*token, true);
            }
        }
    };
    LoggingBlock::LoggingBlock(std::string description, std::optional<Level> overwrite_log_level, EmitEntering emit_entering, EmitLeaving emit_leaving) : 
            block(std::make_unique<LoggingBlockImpl>(std::move(description), overwrite_log_level, emit_leaving)) {
        if (emit_entering == EmitEntering::Always) {
            block->emit();
        }
    }
    LoggingBlock::~LoggingBlock() {
        block->emit_leaving_if_needed();
    }

    void LoggingBlock::emit() {
        block->emit();
    }
}