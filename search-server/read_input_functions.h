#pragma once

#include <string>
#include <vector>

#include "document.h"

std::string ReadLine();

int ReadLineWithNumber();

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);