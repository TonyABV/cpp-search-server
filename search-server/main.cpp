#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "log_duration.h"
#include "process_queries.h"
#include "search_server.h"
#include "test_example_functions.h"

using namespace std;

ostream& operator<<(ostream& os, const DocumentStatus& ds)
{
    switch (ds) {
    case DocumentStatus::ACTUAL: os << "ACTUAL"s; break;
    case DocumentStatus::IRRELEVANT: os << "IRRELEVANT"s; break;
    case DocumentStatus::BANNED: os << "BANNED"s; break;
    case DocumentStatus::REMOVED: os << "REMOVED"s; break;
    }
    return os;
}

template <typename A, typename F>
void RunTestImpl(const A& func, const F& function_name)
{
    func();
    cerr << function_name << " "s << "OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint)
{
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

//Проверка истины
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line, const string& hint)
{
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))



// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {//Проверка добавления документа и нахождении при запросе.
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {//Проверка учета стоп слов
        SearchServer server("in the"s);
        //        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет соответствие документов поисковому запросу.
void TestSearchServerMatched()
{
    const int doc_id = 0;
    const string content = "white cat and  funny collar"s;
    const vector<int> ratings = { 8, -3 };
    {
        //Проверка без стоп слов
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("flurry cat"s, doc_id);
        ASSERT(words[doc_id] == "cat"s);
        ASSERT(words[doc_id] != "flurry"s);
        ASSERT_EQUAL(words.size(), 1);
    }
    {
        //Проверка со стоп словами
        SearchServer server("cat"s);
        //        server.SetStopWords("cat"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("flurry cat"s, doc_id);
        ASSERT_EQUAL(words.size(), 0);
    }
    {
        //Проверка с минус словами
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("flurry -cat"s, doc_id);
        ASSERT_EQUAL(words.size(), 0); //Проверка, что ответ пустой
    }
}

// Проверка сортировки по релевантности
void TestSearchServerRelevanse()
{
    SearchServer server("and in on"s);
    // server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and funny collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "flurry cat flurry tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "lucky dog good eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "lucky starling Eugene"s, DocumentStatus::BANNED, { 9 });

    const auto& documents = server.FindTopDocuments("flurry lucky cat"s);
    int doc_size = documents.size();
    ASSERT_EQUAL(doc_size, 3); //Проверка, что ответ по длине совпадает с ожидаемым
    ASSERT_HINT(is_sorted(documents.begin(), documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        }), "Relevance not sorted correctly"s);
}

// Проверка правильности подсчета рейтинга
void TestSearchServerRating()
{
    //Создали вектор с рейтингами
    vector<vector<int>> ratings = { {8, -3}, {7, 2, 7}, {5, -12, 2, 1}, {9} };
    //Посчитали рейтинги и положили в вектор
    map<int, int> rating_count;
    for (int i = 0; i < ratings.size(); ++i) {
        int n = accumulate(ratings[i].begin(), ratings[i].end(), 0);
        int d = ratings[i].size();
        rating_count[i] = n/d; // Вычисляем рейтинг вручную и привязываем к id
    }

    SearchServer server(""s);
    //server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and  funny collar"s, DocumentStatus::ACTUAL, ratings[0]); // Рейтинг округляется до целого 2
    server.AddDocument(1, "flurry cat flurry tail"s, DocumentStatus::ACTUAL, ratings[1]); // Рейтинг округляется до целого 5
    server.AddDocument(2, "lucky dog good eyes"s, DocumentStatus::ACTUAL, ratings[2]); // Рейтинг округляется до целого -1
    server.AddDocument(3, "lucky starling Eugene"s, DocumentStatus::BANNED, ratings[3]); // Рейтинг 9

    const auto& documents = server.FindTopDocuments("flurry lucky cat"s);
    int doc_size = documents.size();
    for (int i = 0; i < doc_size; ++i) {
        ASSERT_HINT(documents[i].rating == rating_count[documents[i].id], "The rating is calculated incorrectly"s);
    }
}

//Проверка поиска по статусу документа
void TestSearchServerStatus()
{
    SearchServer server("and in on"s);
    // server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and  funny collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "flurry cat flurry tail"s, DocumentStatus::IRRELEVANT, { 7, 2, 7 });
    server.AddDocument(2, "lucky dog good eyes"s, DocumentStatus::BANNED, { 5, -12, 2, 1 });
    server.AddDocument(3, "lucky starling Eugene"s, DocumentStatus::REMOVED, { 9 });
    {// Поверка наличия одного документа со статусом ACTUAL
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents.size(), 1);
        ASSERT(documents[0].id == 0);
    }
    {// Поверка наличия одного документа со статусом IRRELEVANT
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(documents.size(), 1);
        ASSERT(documents[0].id == 1);
    }
    {// Поверка наличия одного документа со статусом BANNED
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(documents.size(), 1);
        ASSERT(documents[0].id == 2);
    }
    {// Поверка наличия одного документа со статусом REMOVED
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(documents.size(), 1);
        ASSERT(documents[0].id == 3);
    }
    {// Поверка наличия одного документа со статусом по умолчанию (ACTUAL)
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s);
        ASSERT_EQUAL(documents.size(), 1);
        ASSERT(documents[0].id == 0);
    }
}

void TestSearchServerPredictate()
{
    SearchServer server("and in on"s);
    // server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and  funny collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "flurry cat flurry tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "lucky dog good eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "lucky starling Eugene"s, DocumentStatus::BANNED, { 9 });
    { //Проверка id документа
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        for (const auto& document : documents) {
            ASSERT_HINT(document.id % 2 == 0, ""s);
        }
    }
    { //Проверка статуса
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
        ASSERT_EQUAL(documents.size(), 3);
    }
    { //Проверка рейтинга
        const auto& documents = server.FindTopDocuments("flurry lucky cat"s, [](int document_id, DocumentStatus status, int rating) { return rating > 3; });
        for (const auto& document : documents) {
            ASSERT_HINT(document.rating > 3, "Rating does not match"s);
        }
    }
}

void TestSearchServerMinus() {
    SearchServer server("and in on"s);
    // server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and  funny collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "flurry cat flurry tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "lucky dog good eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "lucky starling Eugene"s, DocumentStatus::ACTUAL, { 9 });
    {
        const auto& documents = server.FindTopDocuments("flurry -lucky -cat"s);
        ASSERT_EQUAL(documents.size(), 0);
    }
    {
        const auto& documents = server.FindTopDocuments("flurry lucky -cat"s);
        // Проверим, что запрос выдал 2 документа id 2 и 3.
        ASSERT_EQUAL(documents.size(), 2);
        // Проверим, что в документах с выданными id нет минус слова "cat", специально сделав запрос не используя минус слово
        for (const auto& document : documents) {
            const auto [words, status] = server.MatchDocument("flurry lucky cat"s, document.id);
            for (const auto& word : words) {
                ASSERT(word != "cat"s);
            }
        }
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestSearchServerMatched);
    RUN_TEST(TestSearchServerRelevanse);
    RUN_TEST(TestSearchServerRating);
    RUN_TEST(TestSearchServerStatus);
    RUN_TEST(TestSearchServerPredictate);
    RUN_TEST(TestSearchServerMinus);
}

string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        // 97 = 'a', 122 = 'z'
        word.push_back(uniform_int_distribution(97, 122)(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy) {
    LOG_DURATION(string(mark));
    double total_relevance = 0;
    for (const string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    cout << total_relevance << endl;
}

#define TEST(policy) Test(#policy, search_server, queries, execution::policy)

int main() {
    TestSearchServer();

    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, { 1, 2, 3 });
    }

    const auto queries = GenerateQueries(generator, dictionary, 100, 70);

    TEST(seq);
    TEST(par);
}