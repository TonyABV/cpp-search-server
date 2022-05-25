#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "log_duration.h"
#include "process_queries.h"
#include "search_server.h"
#include "test_example_functions.h"
#include "tests.h"

using namespace std;

std::ostream& operator<<(std::ostream& os, const DocumentStatus& ds)
{
    switch (ds) {
    case DocumentStatus::ACTUAL: os << "ACTUAL"s; break;
    case DocumentStatus::IRRELEVANT: os << "IRRELEVANT"s; break;
    case DocumentStatus::BANNED: os << "BANNED"s; break;
    case DocumentStatus::REMOVED: os << "REMOVED"s; break;
    }
    return os;
}

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