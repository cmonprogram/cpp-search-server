#pragma once
#include <string>
#include <vector>
#include <set>
//ѕозвол€ет хранить слова в контейнере string_data_ дл€ того, чтобы string_view не инвалидировались в процессе работы
//используетс€ дл€ хранени€ стоп-слов и слов документов
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

std::vector<std::string_view> SplitIntoWordsCache(std::string_view text, StringCache& database);
std::vector<std::string_view> SplitIntoWordsView(std::string_view text);

template <typename StringContainer>
std::set<std::string_view> MakeUniqueNonEmptyStrings(const StringContainer& strings, StringCache& database) {
    std::set<std::string_view> non_empty_strings;
    for (std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(database.get(str));
        }
    }
    return non_empty_strings;
}