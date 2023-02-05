#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double DELTA = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename FilterFunc>
    vector<Document> FindTopDocuments(const string& raw_query, FilterFunc func)
        const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, func);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < DELTA) {
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


    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus input_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [input_status](int document_id, DocumentStatus status, int rating) { return status == input_status; });
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename FilterFunc>
    vector<Document> FindAllDocuments(const Query& query, FilterFunc filter_func) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (filter_func(document_id,
                    documents_.at(document_id).status,
                    documents_.at(document_id).rating)
                    //documents_.at(document_id).status == status
                    ) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};


template <typename T>
ostream& operator<< (ostream& os, const set<T>& inp) {
    os << "{";
    bool is_first = true;
    for (const auto& val : inp) {
        if (is_first) {
            os << val;
            is_first = false;
        }
        else {
            os << ", " << val;
        }
    }
    os << "}";
    return os;
}

template <typename T>
ostream& operator<< (ostream& os, const vector<T>& inp) {
    os << "{";
    bool is_first = true;
    for (const auto& val : inp) {
        if (is_first) {
            os << val;
            is_first = false;
        }
        else {
            os << ", " << val;
        }
    }
    os << "}";
    return os;
}

template <typename KeyT, typename ValT>
ostream& operator<< (ostream& os, const map<KeyT, ValT>& inp) {
    os << "{";
    bool is_first = true;
    for (const auto& [key, val] : inp) {
        if (is_first) {
            os << key << ": " << val;
            is_first = false;
        }
        else {
            os << ", " << key << ": " << val;
        }
    }
    os << "}";
    return os;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


template <typename FUNCTION>
void RunTestImpl(FUNCTION f, const string& f_name) {
    f();
    cerr << f_name << " OK" << endl;
    /* Напишите недостающий код */
}

#define RUN_TEST(func) RunTestImpl(func, (#func))
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestAddedDocumentContent() {
    //Добавление документов.Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
    {
        SearchServer server;
        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(43, "dog near the village", DocumentStatus::ACTUAL, { 1, 2, 3 });
        vector<Document> found_docs;

        {
            found_docs = server.FindTopDocuments("cat"s);
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 42);
            found_docs = server.FindTopDocuments("cat in"s);
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 42);
            found_docs = server.FindTopDocuments("cat in the"s);
            ASSERT(found_docs.size() == 2);
            ASSERT(found_docs[0].id == 42);
            ASSERT(found_docs[1].id == 43);
            found_docs = server.FindTopDocuments("cat in the city"s);
            ASSERT(found_docs.size() == 2);
            ASSERT(found_docs[0].id == 42);
            ASSERT(found_docs[1].id == 43);
            found_docs = server.FindTopDocuments("   cat    in    the    city fasdf gfg"s);
            ASSERT(found_docs.size() == 2);
            ASSERT(found_docs[0].id == 42);
            ASSERT(found_docs[1].id == 43);
            found_docs = server.FindTopDocuments("cat dog"s);
            ASSERT(found_docs.size() == 2);
            ASSERT(found_docs[0].id == 42);
            ASSERT(found_docs[1].id == 43);
        }
    }
}
void TestStopWords() {
    //Поддержка стоп-слов. Стоп-слова исключаются из текста документов.
    {
        SearchServer server;
        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        vector<Document> found_docs;
        {
            server.SetStopWords("in the"s);
            found_docs = server.FindTopDocuments("in the"s);
            ASSERT(found_docs.size() == 0);

            found_docs = server.FindTopDocuments("cat"s);
            ASSERT(found_docs.size() == 1);
        }
    }
}
void TestMinusWords() {
    //Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
    {
        SearchServer server;
        server.AddDocument(42, "-cat in -the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(43, "dog near the village", DocumentStatus::ACTUAL, { 1, 2, 3 });
        vector<Document> found_docs;
        {
            found_docs = server.FindTopDocuments("cat"s);
            ASSERT(found_docs.size() == 0);

            found_docs = server.FindTopDocuments("the"s);
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 43);
        }
    }
}
void TestMatchedWords() {
    //Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
    {
        SearchServer server;
        server.AddDocument(42, "cat in -the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(43, "dog near the village", DocumentStatus::ACTUAL, { 1, 2, 3 });
        vector<Document> found_docs;
        {


            auto [matched_words, status] = server.MatchDocument("dog near the village", 43);
            ASSERT(status == DocumentStatus::ACTUAL);
            ASSERT(find(matched_words.begin(), matched_words.end(), "dog") != matched_words.end());
            ASSERT(find(matched_words.begin(), matched_words.end(), "near") != matched_words.end());
            ASSERT(find(matched_words.begin(), matched_words.end(), "the") != matched_words.end());
            ASSERT(find(matched_words.begin(), matched_words.end(), "village") != matched_words.end());
        }
        {
            auto [matched_words, status] = server.MatchDocument("-cat in the city", 42);
            ASSERT(status == DocumentStatus::ACTUAL);
            ASSERT(matched_words.size() == 0);
        }
        {
            auto [matched_words, status] = server.MatchDocument("cat in -the city", 42);
            ASSERT(status == DocumentStatus::ACTUAL);
            ASSERT(find(matched_words.begin(), matched_words.end(), "cat") != matched_words.end());
            ASSERT(find(matched_words.begin(), matched_words.end(), "in") != matched_words.end());
            ASSERT(find(matched_words.begin(), matched_words.end(), "city") != matched_words.end());
        }

    }
}
void TestSorting() {
    //Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
    {
        SearchServer server;
        server.AddDocument(41, "cat cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(43, "dog near the village", DocumentStatus::ACTUAL, { 1, 2, 3 });
        vector<Document> found_docs;
        {
            found_docs = server.FindTopDocuments("cat dog"s);
            ASSERT(found_docs.size() == 3);
            ASSERT(found_docs[0].id == 43);
            ASSERT(found_docs[1].id == 41);
            ASSERT(found_docs[2].id == 42);
        }
    }
}
void TestRating() {
    //Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа..
    {
        SearchServer server;
        server.AddDocument(41, "cat cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, -2 });
        server.AddDocument(43, "dog near the village", DocumentStatus::ACTUAL, { 1, -100, 3, 5 });
        vector<Document> found_docs;
        {
            found_docs = server.FindTopDocuments("cat dog"s);
            ASSERT(found_docs.size() == 3);
            ASSERT(found_docs[0].id == 43);
            ASSERT(found_docs[0].rating == -22);
            ASSERT(found_docs[1].id == 41);
            ASSERT(found_docs[1].rating == 2);
            ASSERT(found_docs[2].id == 42);
            ASSERT(found_docs[2].rating == 0);
        }
    }
}
void TestFilter() {
    //Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
    {
        SearchServer server;
        server.AddDocument(41, "cat cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(43, "dog near the village", DocumentStatus::ACTUAL, { 1, 2, 3 });
        vector<Document> found_docs;
        {
            found_docs = server.FindTopDocuments("cat dog"s, [](int document_id, DocumentStatus status, int rating) { return document_id == 42; });
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 42);
        }
    }
}

void TestSearch() {
    //Поиск документов, имеющих заданный статус.
    {
        SearchServer server;
        server.AddDocument(41, "cat cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(42, "cat in the city", DocumentStatus::BANNED, { 1, 2, 3 });
        server.AddDocument(43, "dog near the village", DocumentStatus::IRRELEVANT, { 1, 2, 3 });
        server.AddDocument(44, "dog dog near the village", DocumentStatus::REMOVED, { 1, 2, 3 });
        vector<Document> found_docs;
        {
            found_docs = server.FindTopDocuments("cat dog"s, DocumentStatus::ACTUAL);
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 41);

            found_docs = server.FindTopDocuments("cat dog"s, DocumentStatus::BANNED);
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 42);

            found_docs = server.FindTopDocuments("cat dog"s, DocumentStatus::IRRELEVANT);
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 43);

            found_docs = server.FindTopDocuments("cat dog"s, DocumentStatus::REMOVED);
            ASSERT(found_docs.size() == 1);
            ASSERT(found_docs[0].id == 44);
        }
    }
}
void TestRelevance() {
    //Корректное вычисление релевантности найденных документов.
    {
        SearchServer server;
        server.AddDocument(41, "cat cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(43, "dog near the village", DocumentStatus::ACTUAL, { 1, 2, 3 });
        vector<Document> found_docs;
        {
            found_docs = server.FindTopDocuments("cat dog"s);
            ASSERT(found_docs.size() == 3);
            ASSERT(found_docs[0].id == 43);
            ASSERT(found_docs[0].relevance == 0.27465307216702745);
            ASSERT(found_docs[1].id == 41);
            ASSERT(found_docs[1].relevance == 0.16218604324326577);
            ASSERT(found_docs[2].id == 42);
            ASSERT(found_docs[2].relevance == 0.10136627702704110);
        }
    }
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddedDocumentContent);
    RUN_TEST(TestStopWords);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchedWords);
    RUN_TEST(TestSorting);
    RUN_TEST(TestRating);
    RUN_TEST(TestFilter);
    RUN_TEST(TestSearch);
    RUN_TEST(TestRelevance);

}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}