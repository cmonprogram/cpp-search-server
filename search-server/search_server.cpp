#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <tuple>
#include <execution>
#include <mutex>
#include <future>

#include "search_server.h"
#include "string_processing.h"


SearchServer::SearchServer() = default;
SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWordsView(stop_words_text)) {}
SearchServer::SearchServer(std::string_view stop_words_text) : SearchServer(SplitIntoWordsView(std::string(stop_words_text))) {}//SplitIntoWordsCache

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {

    //checks
    if (document_id < 0) throw std::invalid_argument("ID less than zero");
    if (documents_.count(document_id) > 0) throw std::invalid_argument("ID is not exist");

    const std::vector<std::string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    DocumentData document_data = { ComputeAverageRating(ratings), status};
    for (std::string_view word : words) {
        if (!IsValidWord(word)) throw std::invalid_argument("Forbidden symbols");
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, document_data);
    documents_index_.insert(document_id);
}


std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    const static std::map<std::string_view, double> static_map;
    if (!DocumeentExist(document_id)) return static_map;
    return document_to_word_freqs_.at(document_id);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

bool SearchServer::DocumeentExist(int document_id) const{
    return std::binary_search(documents_index_.begin(), documents_index_.end(), document_id);
}

std::set<int>::const_iterator SearchServer::begin() {
    return documents_index_.begin();
}

std::set<int>::const_iterator SearchServer::end() {
    return documents_index_.end();
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);


    std::vector<std::string_view> matched_words;


    for (std::string_view word : query.minus_words) {

        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return std::tuple{ std::vector<std::string_view> {}, documents_.at(document_id).status };
            //matched_words.clear();
            //break;
        }
    }

    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return std::tuple{ matched_words, documents_.at(document_id).status };
}
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy policy, const std::string& raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy policy, const std::string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query, true);

    if (std::any_of(
        std::execution::par,
        query.minus_words.begin(), query.minus_words.end(),
        [&](std::string_view word) {
            return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id) != 0;
        }
    )) {
        return std::tuple{ std::vector<std::string_view> {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());

    auto end_it = std::copy_if(
        std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [&](std::string_view word) {
            return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id) != 0;
        }
    );

    matched_words.erase(end_it, matched_words.end());

    std::sort(policy, matched_words.begin(), matched_words.end());
    auto end_it2 = std::unique(policy, matched_words.begin(), matched_words.end());
    matched_words.erase(end_it2, matched_words.end());

    return std::tuple{ matched_words, documents_.at(document_id).status };

}



void SearchServer::RemoveDocument(int document_id) {
    if (document_to_word_freqs_.count(document_id) == 0) return;
    for (auto& [word_, freqs_]  : word_to_document_freqs_) {
        //for (auto it = freqs_.begin(); it != freqs_.end(); ++it) {}
        freqs_.erase(document_id);
        //word_to_document_freqs_[word_] = freqs_;
    }
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    documents_index_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy policy, int document_id) {
    RemoveDocument(document_id);
}
void SearchServer::RemoveDocument(std::execution::parallel_policy policy, int document_id) {
    if (document_to_word_freqs_.count(document_id) == 0) return;

    //std::map<std::string, double>& words_to_delete_map = document_to_word_freqs_[document_id];
    std::vector<std::string_view> words_to_delete_vector(document_to_word_freqs_[document_id].size());

    std::transform(
        std::execution::par,
        document_to_word_freqs_[document_id].begin(), document_to_word_freqs_[document_id].end(),
        words_to_delete_vector.begin(),
        [](const auto& val) { 
            return val.first;
        }
    );


    std::for_each(
        std::execution::par,
        words_to_delete_vector.begin(), words_to_delete_vector.end(),
        [this, &document_id](const auto& word_to_delete) {
            word_to_document_freqs_.at(word_to_delete).erase(document_id);
        }
    );

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    documents_index_.erase(document_id);
}


bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(std::string(word)) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWordsCache(text,string_data_)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    /*
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    */
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}


SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    bool is_minus = false;
    size_t size = text.size();
    if (size == 0) throw std::invalid_argument("Empty text");
    if (!IsValidWord(text)) throw std::invalid_argument("Forbidden symbols");

    if (text[0] == '-') {
        if (size == 1) throw std::invalid_argument("Empty minus word");
        if (text[1] == '-') throw std::invalid_argument("Forbidden minus word");
        is_minus = true;
        text = text.substr(1);
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool duplicates) const {
    Query result;
    for (std::string_view word : SplitIntoWordsView(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if (!duplicates) {
        std::sort(result.minus_words.begin(), result.minus_words.end());
        auto new_end_minus = std::unique(result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(new_end_minus, result.minus_words.end());

        std::sort(result.plus_words.begin(), result.plus_words.end());
        auto new_end_plus = std::unique(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(new_end_plus, result.plus_words.end());
    }
    return result;
}


// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}