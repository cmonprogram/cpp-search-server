#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <execution>
#include <atomic>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double DELTA = 1e-6;

class SearchServer {
public:
    SearchServer();

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    int GetDocumentCount() const;
    bool DocumeentExist(int document_id) const;
    std::set<int>::const_iterator begin();
    std::set<int>::const_iterator end();

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy policy, const std::string& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy policy, const std::string& raw_query, int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy policy, int document_id);

private:
    StringCache string_database;

    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::map<std::string_view, double> freqs;
    };
    
    const std::set<std::string_view> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> documents_index_;

    bool IsStopWord(std::string_view word) const;
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text);
    static int ComputeAverageRating(const std::vector<int>& ratings);
    static bool IsValidWord(std::string_view word);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const;
    SearchServer::Query ParseQuery(std::execution::parallel_policy policy, std::string_view text) const;
    Query ParseQueryWithDuplicates(std::string_view text) const;
    SearchServer::Query ParseQueryWithDuplicates(std::execution::parallel_policy policy, std::string_view text) const;
    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words, string_database)) {
    for (const std::string& word : stop_words) {
        if (!IsValidWord(word)) throw std::invalid_argument("Forbidden symbols");
    }
}


template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {

    Query query = ParseQuery(std::execution::par,raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

    std::sort(std::execution::par_unseq,matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < DELTA) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;

    /*Начните с обучения всех версий методов FindTopDocuments и FindAllDocuments приёму аргументов execution::seq и execution::par.
    Попрофилируйте и определите, где узкое место: в сортировке документов или в FindAllDocuments в циклах по плюс- и минус-словам?
    Вам пригодится ConcurrentMap и, возможно, ConcurrentSet.
    */
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(query, document_predicate);

    std::sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < DELTA) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
    DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
    std::execution::parallel_policy policy,
    const Query& query,
    DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(10000);


    std::for_each(
        std::execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        [&](const auto& word) {
            if (word_to_document_freqs_.count(word) != 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                std::for_each(
                    std::execution::seq,
                    word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(),
                    [&](const auto& map) {
                        const auto& [document_id, term_freq] = map;
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }
                );
            }
        });


    std::for_each(
        std::execution::par_unseq,
        query.minus_words.begin(), query.minus_words.end(),
        [&](const auto& word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.erase(document_id);
                }
            }
        });

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}