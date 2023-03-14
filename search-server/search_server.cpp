#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <algorithm>

#include "search_server.h"
#include "string_processing.h"


SearchServer::SearchServer() = default;
SearchServer::SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)) {}


void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {

    //checks
    if (document_id < 0) throw std::invalid_argument("ID less than zero");
    if (documents_.count(document_id) > 0) throw std::invalid_argument("ID is not exist");

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    DocumentData document_data = { ComputeAverageRating(ratings), status, {}};
    for (const std::string& word : words) {
        if (!IsValidWord(word)) throw std::invalid_argument("Forbidden symbols");
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_data.freqs[word] += inv_word_count;
    }
    documents_.emplace(document_id, document_data);
    documents_index_.push_back(document_id);
}



std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (!DocumeentExist(document_id)) return *new std::map<std::string, double>;
    return documents_.at(document_id).freqs;
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

bool SearchServer::DocumeentExist(int document_id) const{
    return std::binary_search(documents_index_.begin(), documents_index_.end(), document_id);
}

std::vector<int>::const_iterator SearchServer::begin() {
    return documents_index_.begin();
}

std::vector<int>::const_iterator SearchServer::end() {
    return documents_index_.end();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return std::tuple{ matched_words, documents_.at(document_id).status };
}

void SearchServer::RemoveDocument(int document_id) {
    if (!DocumeentExist(document_id)) return;
    for (auto& [word_, freqs_]  : word_to_document_freqs_) {
        //for (auto it = freqs_.begin(); it != freqs_.end(); ++it) {}
        freqs_.erase(document_id);
        //word_to_document_freqs_[word_] = freqs_;
    }
    documents_.erase(document_id);
    documents_index_.erase(std::remove(documents_index_.begin(), documents_index_.end(), document_id));
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
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
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(),0);
    return rating_sum / static_cast<int>(ratings.size());
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}


SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
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

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}