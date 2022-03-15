#pragma once

#include <deque>
#include <string>
#include <vector>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) ;
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query) ;
    
    int GetNoResultRequests() const ;
    
private:
    std::deque<int> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& server_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> result = RequestQueue::server_.FindTopDocuments(raw_query, document_predicate);
    if (requests_.size()==min_in_day_)
    {
        requests_.pop_back();
    }
    if (result.empty()) {
        requests_.push_front({ 1 });
    }
    else {
        requests_.push_front({ 0 });
    }
    return result;
}