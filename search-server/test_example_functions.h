#pragma once
#include "search_server.h"
#include "document.h"

#include <string>
#include <vector>

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);