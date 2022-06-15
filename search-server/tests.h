#pragma once
#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "document.h"
#include "search_server.h"


template <typename A, typename F>
void RunTestImpl(const A& func, const F& function_name)
{
    func();
    std::cerr << function_name << " "s << "OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str,
    const std::string& file, const std::string& func, unsigned line, const std::string& hint)
{
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

//Проверка истины
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
    unsigned line, const std::string& hint)
{
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))



// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent()
{
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
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
    const std::string content = "white cat and  funny collar"s;
    const std::vector<int> ratings = { 8, -3 };
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
    std::vector<std::vector<int>> ratings = { {8, -3}, {7, 2, 7}, {5, -12, 2, 1}, {9} };
    //Посчитали рейтинги и положили в вектор
    std::map<int, int> rating_count;
    for (int i = 0; i < ratings.size(); ++i) {
        int n = accumulate(ratings[i].begin(), ratings[i].end(), 0);
        int d = ratings[i].size();
        rating_count[i] = n / d; // Вычисляем рейтинг вручную и привязываем к id
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

std::string GenerateWord(std::mt19937& generator, int max_length) {
    const int length = std::uniform_int_distribution(1, max_length)(generator);
    std::string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        // 97 = 'a', 122 = 'z'
        word.push_back(std::uniform_int_distribution(97, 122)(generator));
    }
    return word;
}

std::vector<std::string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length) {
    std::vector<std::string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

std::string GenerateQuery(std::mt19937& generator, const std::vector<std::string>& dictionary, int word_count, double minus_prob = 0) {
    std::string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (std::uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[std::uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

std::vector<std::string> GenerateQueries(std::mt19937& generator, const std::vector<std::string>& dictionary, int query_count, int max_word_count) {
    std::vector<std::string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename ExecutionPolicy>
void Test(std::string_view mark, const SearchServer& search_server, const std::vector<std::string>& queries, ExecutionPolicy&& policy) {
    LOG_DURATION(std::string(mark));
    double total_relevance = 0;
    for (const std::string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    std::cout << total_relevance << std::endl;
}

#define TEST(policy) Test(#policy, search_server, queries, std::execution::policy)