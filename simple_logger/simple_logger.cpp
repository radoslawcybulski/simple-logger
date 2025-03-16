#include "simple_logger.h"
#include <cassert>
#include <stdlib.h>

namespace SimpleLogger {
    static std::shared_ptr<ConsumerInterface> ci;
    static thread_local std::string thread_name;
    static thread_local std::string buffer = []() {
        std::string tmp;
        tmp.reserve(4096);
        return tmp;
    }();

    void initialize(std::shared_ptr<ConsumerInterface> ci_) {
        assert(ci_);
        if (ci_)
            ci = std::move(ci_);
    }

    void name_current_thread(std::string name)
    {
        thread_name = std::move(name);
    }

    std::string_view ConsumerInterface::current_thread_name() {
        return thread_name;
    }
    std::string &ConsumerInterface::thread_local_string_buf() {
        return buffer;
    }
    ConsumerInterface &ConsumerInterface::current() {
        return *ci;
    }
}