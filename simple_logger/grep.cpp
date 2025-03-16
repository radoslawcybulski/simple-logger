#include "grep.h"
#include <format>
#include <cassert>
#include "simple_logger.h"

namespace SimpleLogger {
    std::tuple<bool, size_t, std::uint32_t, std::uint32_t> Filter::parse_token_index(std::string_view line, size_t line_index) {
        size_t pos = 0;
        std::uint32_t from = 0;
        if (line.starts_with("##!# ")) {
            pos = 5;
            std::uint32_t to = 0;
            while(pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
                from = from * 10 + (line[pos] - '0');
                ++pos;
            }
            if (line.substr(pos, 2) != "->")
                throw std::runtime_error(std::format("Invalid line {} in log file - expected '->' after first number", line_index));

            pos += 2;
            while(pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
                to = to * 10 + (line[pos] - '0');
                ++pos;
            }
            if (pos == line.size() || line[pos] != ' ')
                throw std::runtime_error(std::format("Invalid line {} in log file - expected space after second number", line_index));

            SimpleLogger::trace("Parsed line {}: `{}` -> {} {} {} {}", line_index, line, false, pos, from, to);
            return std::make_tuple(false, pos, from, to);
        }

        while(pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
            from = from * 10 + (line[pos] - '0');
            ++pos;
        }
        if (pos == line.size() || line[pos] != ' ')
            throw std::runtime_error(std::format("Invalid line {} in log file - expected space after number", line_index));

        SimpleLogger::trace("Parsed line {}: `{}` -> {} {} {} {}", line_index, line, true, pos, from, 0);
        return std::make_tuple(true, pos, from, 0);
    }

    void Filter::initialize_with_line(std::string_view line, size_t line_index) {
        auto [is_msg, pos, from, to] = parse_token_index(line, line_index);
        if (!is_msg) {
            SimpleLogger::trace("Line {}: Adding links from {} to {}", line_index, from, to);
            children.emplace(from, to);
            parents.emplace(to, from);
        }
    }
    void Filter::insert_into_valid_blocks(std::uint32_t block_id) {
        {
            auto it = valid_blocks.emplace(block_id, true);
            if (!it.second && it.first->second) {
                SimpleLogger::trace("insert_into_valid_blocks: block {} is already valid, skipping", block_id);
                return;
            }
            SimpleLogger::trace("insert_into_valid_blocks: marking block {} as valid", block_id);
            it.first->second = true;
        }
        auto it = children.find(block_id);
        while(it != children.end() && it->first == block_id) {
            SimpleLogger::trace("insert_into_valid_blocks: block {}: processing child {}", block_id, it->second);
            insert_into_valid_blocks(it->second);
            ++it;
        }
    }
    void Filter::mark_parents_as_needed(std::uint32_t block_id) {
        if (!valid_blocks.emplace(block_id, false).second) {
            SimpleLogger::trace("mark_parents_as_needed: block {} is already marked as needed, skipping", block_id);
            return;
        }
        auto it = parents.find(block_id);
        while(it != parents.end() && it->first == block_id) {
            SimpleLogger::trace("mark_parents_as_needed: block {}: processing parent {}", block_id, it->second);
            mark_parents_as_needed(it->second);
            ++it;
        }
    }
    void Filter::set_block_ids(std::span<std::uint32_t> block_ids)
    {
        for(auto block_id : block_ids) {
            SimpleLogger::trace("set_block_ids: processing block {}", block_id);
            mark_parents_as_needed(block_id);
            insert_into_valid_blocks(block_id);
        }
    }
    bool Filter::filter(std::string_view line, size_t line_index)
    {
        SimpleLogger::trace("Filtering line {}: {}", line_index, line);
        auto [is_msg, pos, from, to] = parse_token_index(line, line_index);
        auto it1 = valid_blocks.find(from);
        if (it1 == valid_blocks.end()) {
            SimpleLogger::trace("Filtering line {}: removing because block from {} is not in valid blocks", line_index, from);
            return true;
        }
        if (it1->second) {
            SimpleLogger::trace("Filtering line {}: keeping because block from {} is valid", line_index, from);
            return false;
        }
        if (is_msg) {
            SimpleLogger::trace("Filtering line {}: removing because it is a message and block from {} is not valid", line_index, from);
            return true;
        }
        auto it2 = valid_blocks.find(to);
        if (it2 == valid_blocks.end()) {
            SimpleLogger::trace("Filtering line {}: removing because block to {} is not in valid blocks", line_index, to);
            return true;
        }
        SimpleLogger::trace("Filtering line {}: keeping (default)", line_index);
        return false;
    }
}