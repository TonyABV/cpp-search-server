#include "search_server.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;


// -------- Начало модульных тестов поисковой системы ----------
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
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

//  Тест проверяет, что поисковая система исключает документы с минус словами
void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("-cat"s).empty(), "A document whith -word must be excluded.");
    }
}

//  Тест проверяет, что поисковая система при метчинге документа по поисковому запросу возвращает все слова из поискового запроса,
//  присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов
void TestMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<string> expected_matching = { "cat", "in","the" };
        ASSERT_EQUAL((get<vector<string>>(server.MatchDocument("cat in the vilage", 42))), expected_matching);
        ASSERT_HINT((get<vector<string>>(server.MatchDocument("-cat dog in the city", 42))).empty(), "A document whith \"-word\" must be empty.");
    }
}

//  Тест проверяет, что поисковая система cортирует найденные документы в порядке убывания релевантности
void TestSortByRelevance() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    const int sec_doc_id = 52;
    const string sec_content = "dog in the vilage"s;
    const vector<int> sec_ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(sec_doc_id, sec_content, DocumentStatus::ACTUAL, sec_ratings);

        vector<Document> result = server.FindTopDocuments("cat in the city"s);
        ASSERT_HINT(result[0].relevance >= result[1].relevance,"Documents must be sorted by relevance in descending order");
    }
}

//  Тест проверяет, что поисковая система корректно рассчитывает средний рейтинг
void TestCalculateRating() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat"s)[0].rating, (1 + 2 + 3) / 3);
    }
}

//  Тест проверяет, что поисковая система выделяет документы на основании ф-и предикаты
void TestFilterByPredicate() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        ASSERT(server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; }).empty());
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        ASSERT(server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id == 33; }).empty());
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        ASSERT(server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return rating >= 10; }).empty());
    }
}

//  Тест проверяет, что поисковая система выделяет документы при соответствии заданному статусу
void TestSerchByStatus() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat"s, DocumentStatus::BANNED).empty());
    }
}

//  Тест проверяет, что поисковая система правильно рассчитывает рейтинг
void TestCalculateRelevance() {
    const int doc_id = 42;
    const string content = "cat whith collar in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    const int sec_doc_id = 52;
    const string sec_content = "dog whith collar in the vilage"s;
    const vector<int> sec_ratings = { 1, 2, 3 };

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(sec_doc_id, sec_content, DocumentStatus::ACTUAL, sec_ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat"s)[0].relevance, log(2.0 / 1.0) * 1.0 / 6.0);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestMatching);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestCalculateRating);
    RUN_TEST(TestFilterByPredicate);
    RUN_TEST(TestSerchByStatus);
    RUN_TEST(TestCalculateRelevance);
}
// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
