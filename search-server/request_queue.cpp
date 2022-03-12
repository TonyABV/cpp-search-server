#include "request_queue.h"

using namespace std;

    RequestQueue::RequestQueue(const SearchServer& search_server) :server_(search_server)  {
    }

    vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
        return RequestQueue::AddFindRequest(raw_query, [status](int document_id,
                                                 DocumentStatus document_status, int rating) {
                                                    return document_status == status;});
    }

    vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
        return RequestQueue::AddFindRequest(raw_query, DocumentStatus::ACTUAL);
    }

    int RequestQueue::GetNoResultRequests() const {
        return count(requests_.begin(),requests_.end(),1);// напишите реализацию
    }