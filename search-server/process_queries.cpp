#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(),
        [&search_server](std::string query) {
            return search_server.FindTopDocuments(query);
        });
    return result;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::list<Document> result;

    std::vector<std::vector<Document>> results(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), results.begin(),
        [&search_server](std::string query) {
            return search_server.FindTopDocuments(query);
        });

    for (auto v_d : results) {
        result.insert(result.end(), v_d.begin(), v_d.end());
    }
    return result;
}