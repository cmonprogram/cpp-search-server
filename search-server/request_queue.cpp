#include <algorithm>

#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : search_server_{ search_server } {}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
    requests_.push_back({ result });
    if (requests_.size() - 1 == min_in_day_) requests_.pop_front();
    return result;
}
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query);
    requests_.push_back({ result });
    if (requests_.size() - 1 == min_in_day_) requests_.pop_front();
    return result;
}
int RequestQueue::GetNoResultRequests() const {
    return std::count_if(requests_.begin(), requests_.end(), [](const QueryResult& item) { return item.documents.empty(); });
}