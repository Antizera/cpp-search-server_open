#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>
using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

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

template <typename T>
void RunTestImpl(const T& value, const string& expr_str) {
    value();
    cerr << expr_str << " OK"s << endl;
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(func)  RunTestImpl((func), #func)


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
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    SearchServer(const string& str) : SearchServer(SplitIntoWords(str)) {
    }

    template <typename StringContainer>
    SearchServer(const StringContainer container) {
        if (all_of(container.begin(), container.end(), [](const string& str) {return !IsValidWord(str); }))
            throw invalid_argument("Invalid symbol in stop words");
        for (const string& word : container)
            stop_words_.insert(word);
    }




    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        if (document_id < 0 || documents_.count(document_id) == 1 || !IsValidWord(document))
            throw invalid_argument("Invalid symbol in stop words or invalid id");

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        documents_id_.push_back(document_id);
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {

        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status; });

    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);

    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    inline static constexpr int INVALID_DOCUMENT_ID = -1;
    int GetDocumentId(int index) const {
        if (index > documents_id_.size())
            throw out_of_range("Out of range");
        return documents_id_[index - 1];
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {

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
    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    vector <int> documents_id_;
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
        for (const int rating : ratings) {
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
        if (!IsValidWord(text))
            throw invalid_argument("Invalid symbol");

        if (text[0] == '-') {
            if (text.size() == 1 || text[1] == '-')
                throw invalid_argument("Invalid symbol after -");
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

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

//==================TESTS==================

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        vector <string> stop = { "in"s, "the"s };
        SearchServer server(stop);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}


void TestAddDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(!server.FindTopDocuments("cat"s).empty());
    }
}

void TestMinusWord() {
    const int doc_id = 42;
    const string content = "-cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat"s).empty());
    }
}

void TestMatch() {
    const int doc_id = 42;
    const string content = "-cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat"s).empty());
        server.AddDocument(1, "cat"s, DocumentStatus::ACTUAL, { 1, 32 });
    }
}

void TestSort() {
    const string content = "1"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("in the"s);
        server.AddDocument(3, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "1 2 "s, DocumentStatus::ACTUAL, { 1, 32 });
        server.AddDocument(1, "1 2 3"s, DocumentStatus::ACTUAL, { 1 });
        vector<Document> docs = server.FindTopDocuments("1 2 3"s);
        ASSERT(docs[0].relevance > docs[1].relevance && docs[1].relevance > docs[2].relevance);
    }
}

void TestAvgRaiting() {
    SearchServer server("in the"s);
    server.AddDocument(1, "1"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(2, "1 2"s, DocumentStatus::ACTUAL, { -2, -3, -1 });
    server.AddDocument(3, "1 2 3"s, DocumentStatus::ACTUAL, { 3, 3, 3 });
    server.AddDocument(4, "1 2 3 4"s, DocumentStatus::ACTUAL, { -1, 1, -3 });
    vector<Document> docs = server.FindTopDocuments("1"s);
    ASSERT_EQUAL(docs[0].rating, 3);
    ASSERT_EQUAL(docs[1].rating, 2);
    ASSERT_EQUAL(docs[2].rating, -1);
    ASSERT_EQUAL(docs[3].rating, -2);
}

void TestStatus() {
    SearchServer server("in the"s);
    server.AddDocument(1, "1"s, DocumentStatus::BANNED, { 1, 2, 3 });
    server.AddDocument(2, "1 2"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(3, "1 2 3"s, DocumentStatus::IRRELEVANT, { 1, 2, 3 });
    server.AddDocument(4, "1 2 3 4"s, DocumentStatus::REMOVED, { 1, 2, 3 });
    ASSERT(!server.FindTopDocuments("1", DocumentStatus::BANNED).empty());
    ASSERT(!server.FindTopDocuments("1", DocumentStatus::ACTUAL).empty());
    ASSERT(!server.FindTopDocuments("1", DocumentStatus::IRRELEVANT).empty());
    ASSERT(!server.FindTopDocuments("1", DocumentStatus::REMOVED).empty());
}

void TestPredicate() {
    SearchServer server("in the"s);
    server.AddDocument(1, "1 2 3"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    ASSERT(server.FindTopDocuments("1 2 3", [](int document_id, DocumentStatus status, int rating) {return document_id > 1; }).empty());
}

void TestRel() {
    SearchServer server("in the"s);
    server.AddDocument(1, "1"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(2, "1 2"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(23, " 2 3"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    vector<Document> docs = server.FindTopDocuments("1"s);
    ASSERT_EQUAL(round(docs[0].relevance * 10000000000) / 10000000000, 0.4054651081);
    ASSERT_EQUAL(round(docs[1].relevance * 10000000000) / 10000000000, 0.2027325541);
}

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestMinusWord);
    RUN_TEST(TestMatch);
    RUN_TEST(TestSort);
    RUN_TEST(TestAvgRaiting);
    RUN_TEST(TestStatus);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestRel);
}


// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}
int main() {

    SearchServer search_server("и в на"s);
    TestSearchServer();

    search_server.AddDocument(123, "белый кот и модный ошейник", DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(0, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("ухоженный кот")) {
        PrintDocument(document);
    }

    cout << "ACTUAL:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }


    cout << search_server.GetDocumentId(4);
}