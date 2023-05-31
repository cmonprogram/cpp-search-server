#include <string_view>
#include "string_processing.h"
std::vector<std::string_view> SplitIntoWordsView(std::string_view text) {
    std::vector<std::string_view> words;
    std::string_view::iterator begin = text.begin();
    std::ptrdiff_t i = 0;
    for (const char c : text) {
        if (c == ' ') {
            if (i != 0) {
                words.push_back(std::string_view(&(*begin),i));
                std::advance(begin, i);
                i = 0;
            }
                std::advance(begin, 1);
        }
        else {
            ++i;
        }
    }
    if (i != 0) {
        words.push_back(std::string_view(&(*begin), i));
    }

    return words;
}