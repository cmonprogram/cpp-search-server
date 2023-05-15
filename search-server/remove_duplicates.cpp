#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<std::set<std::string_view>> unique;
    std::vector<int> remove_list;
    for (const int document_id : search_server) {
        const std::map<std::string_view, double> freqs = search_server.GetWordFrequencies(document_id);
        std::set<std::string_view> words;
        for (const auto& [key, value] : freqs) {
            words.insert(key);
        }
        auto u_it = unique.find(words);
        if (u_it != unique.end()) {
            remove_list.push_back(document_id);
        }
        else {
            unique.insert(words);
        }
    }

    for (int id : remove_list) {
            std::cout << "Found duplicate document id " << id << std::endl;
            search_server.RemoveDocument(id);
        }
    }