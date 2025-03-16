#ifndef RC_TESTS_MAIN
#define RC_TESTS_MAIN

#include <gtest/gtest.h>

namespace SimpleLogger {
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

#endif