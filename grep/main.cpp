#include <fstream>
#include <iostream>
#include "simple_logger/grep.h"
#include <vector>
#include <string_view>
#include <string>

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <log file> block-id block-id..." << std::endl;
        std::cerr << "Prints all log messages that are inside any of the specified blocks" << std::endl;
        return 1;
    }
    std::vector<std::uint32_t> block_ids;
    for(int i = 2; i < argc; ++i) {
        try {
            block_ids.push_back(std::stoul(argv[i]));
        }
        catch(...) {
            std::cerr << "Invalid block id: " << argv[i] << std::endl;
            return 1;
        }
    }

    SimpleLogger::Filter filter;
    {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Failed to open file: " << argv[1] << std::endl;
            return 1;
        }
        std::string line;
        size_t line_index = 0;
        while(std::getline(file, line)) {
            ++line_index;
            filter.initialize_with_line(line, line_index);
        }
    }

    filter.set_block_ids(block_ids);
    {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Failed to open file: " << argv[1] << std::endl;
            return 1;
        }
        std::string line;
        size_t line_index = 0;
        while(std::getline(file, line)) {
            ++line_index;
            if (!filter.filter(line, line_index)) {
                std::cout << line << std::endl;
            }
        }
    }
    return 0;
}