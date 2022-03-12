#include "process_queries.h"
#include "search_server.h"

#include <execution>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

//void PrintDocument(const Document& document) {
//    cout << "{ "s
//        << "document_id = "s << document.id << ", "s
//        << "relevance = "s << document.relevance << ", "s
//        << "rating = "s << document.rating << " }"s << endl;
//}

ostream& operator<<(ostream& os, const DocumentStatus& ds) {
    switch (ds) {
    case DocumentStatus::ACTUAL: os << "ACTUAL"s; break;
    case DocumentStatus::IRRELEVANT: os << "IRRELEVANT"s; break;
    case DocumentStatus::BANNED: os << "BANNED"s; break;
    case DocumentStatus::REMOVED: os << "REMOVED"s; break;
    }
    return os;
}




template <typename A, typename F>
void RunTestImpl(const A& func, const F& function_name) {

    func();
    cerr << function_name << " "s << "OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
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
void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line, const string& hint) {
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
void TestExcludeStopWordsFromAddedDocumentContent() {
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
void TestSearchServerMatched() {
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
void TestSearchServerRelevanse() {
    SearchServer server("and in on"s);
    //    server.SetStopWords("and in on"s);
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
void TestSearchServerRating() {
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
void TestSearchServerStatus() {
    SearchServer server("and in on"s);
    //    server.SetStopWords("and in on"s);
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

void TestSearchServerPredictate() {
    SearchServer server("and in on"s);
    //    server.SetStopWords("and in on"s);
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
    //    server.SetStopWords("and in on"s);
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

void TestSearchServerCalcRelevance() {
    SearchServer server("and in on"s);
    //    server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and  funny collar"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "flurry cat flurry tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "lucky dog good eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "lucky starling Eugene"s, DocumentStatus::BANNED, { 9 });
    //Подготовка для ручного вычисления TF-IDF
    //Разбиваем документы на отдельные слова убирая стоп слова
    //Расчет ведется до отбрасывания слов по статусу
    map<int, vector<string>> doc_for_tf_idf = { {0, {"white"s,"cat"s, "funny"s, "collar"s }},
                                               {1, {"flurry"s,"cat"s, "flurry"s, "tail"s }},
                                               {2, {"lucky"s,"dog"s, "good"s, "eyes"s }},
                                               {3, {"lucky"s,"starling"s, "Eugene"s }} };
    //Разбиваем запрос на отдельные слова
    map<int, string> query = { {0, "flurry"s}, {1, "lucky"s}, {2, "cat"s} };
    map<int, double> query_idf; // ключ id слова из запроса
    map<int, map<int, double>> doc_tf;
    //Проверяем встречается ли слово в документе или нет, не важно сколько раз.
    //Далее считаем IDF для каждого слова по формуле idf=log(количество документов / количество совпадений)
    //Например слово "flurry" встречается в одном документе, значит его idf=log(4/1)
    for (int i = 0; i < query.size(); ++i) {
        int freq_query_count = 0; //Обнуляется при переходе к следующему слову
        doc_tf[i];
        for (int j = 0; j < doc_for_tf_idf.size(); ++j) {
            //Проверяем есть ли слово в документе, не важно сколько раз
            if (count(doc_for_tf_idf.at(j).begin(), doc_for_tf_idf.at(j).end(), query.at(i))) {
                ++freq_query_count;
            }
            //Вычисляем tf, ключ номер слова. В контейнере значений, ключ номер документа
            doc_tf.at(i)[j] = count(doc_for_tf_idf.at(j).begin(), doc_for_tf_idf.at(j).end(), query.at(i)) * 1.0 / doc_for_tf_idf.size();
        }
        //Вычисляем idf, ключ номер слова
        query_idf[i] = log(doc_for_tf_idf.size() / freq_query_count);
    }
    //Перемножаем tf и idf и складываем по документам, ключ номер документа
    map<int, double> tf_idf;
    for (int j = 0; j < doc_for_tf_idf.size(); ++j) {
        for (int i = 0; i < query_idf.size(); ++i) {
            tf_idf[j] += doc_tf.at(i).at(j) * query_idf.at(i);
        }
    }

    const auto& documents = server.FindTopDocuments("flurry lucky cat"s);
    for (const Document& document : documents) {
        double diff = abs(document.relevance - tf_idf.at(document.id));
        cout << document.id << " : "s << diff << endl;
        ASSERT(abs(document.relevance - tf_idf.at(document.id)) < 1e-6);
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
    //RUN_TEST(TestSearchServerCalcRelevance);

}

int main() {
    TestSearchServer();
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }

    cout << "ACTUAL by default:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    // последовательная версия
    for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    // параллельная версия
    for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}