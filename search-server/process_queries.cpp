#include "process_queries.h"
#include <numeric>
#include <execution>
#include <iterator>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result(queries.size());

    std::transform(
        std::execution::par,
        queries.begin(), queries.end(),
        result.begin(),
        [&search_server](const std::string& query) {return search_server.FindTopDocuments(query); }
    );

    return result;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<Document> result;
    result = std::transform_reduce(
        std::execution::par,
        queries.begin(), queries.end(),
        result,
        [](std::vector<Document> res, const std::vector<Document>& documents) {
            std::move(documents.begin(), documents.end(), std::back_inserter(res));
            return res;
        },
        [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); }
    );
    return result;
}
