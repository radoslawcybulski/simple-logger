#ifndef RC_SIMPLE_LOGGER_LIB_GREP_H
#define RC_SIMPLE_LOGGER_LIB_GREP_H

#include <unordered_map>
#include <cstdint>
#include <optional>
#include <tuple>
#include <string_view>
#include <span>

namespace SimpleLogger {
    class Filter {
        std::unordered_multimap<std::uint32_t, std::uint32_t> children;
        std::unordered_multimap<std::uint32_t, std::uint32_t> parents;
        std::unordered_map<std::uint32_t, bool> valid_blocks;

        std::tuple<bool, size_t, std::uint32_t, std::uint32_t> parse_token_index(std::string_view line, size_t line_index);
        void insert_into_valid_blocks(std::uint32_t block_id);
        void mark_parents_as_needed(std::uint32_t block_id);
        
    public:
        void initialize_with_line(std::string_view line, size_t line_index);
        void set_block_ids(std::span<std::uint32_t> block_ids);
        bool filter(std::string_view line, size_t line_index);
    };
}

#endif