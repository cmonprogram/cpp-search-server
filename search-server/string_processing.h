#pragma once
#include <string>
#include <vector>
#include <set>
class StringCache {
public:
    std::string_view get(std::string_view inp) {
        auto result = string_data_.find(inp);
        if (result != string_data_.end()) {
            return *result;
        }
        else {
            auto result = string_data_.insert(std::string(inp));
            return *(result.first);
        };
    }
private:
    std::set<std::string, std::less<>> string_data_;
};

std::vector<std::string> SplitIntoWords(const std::string& text);
std::vector<std::string_view> SplitIntoWordsCache(std::string_view text, StringCache& database);
std::vector<std::string_view> SplitIntoWordsView(std::string_view text);

template <typename StringContainer>
std::set<std::string_view> MakeUniqueNonEmptyStrings(const StringContainer& strings, StringCache& database) {
    std::set<std::string_view> non_empty_strings;
    for (const std::string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(database.get(str));
        }
    }
    return non_empty_strings;
}