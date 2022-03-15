#include "search_server.h"

#include <iterator>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : stor_stop_words(stop_words_text),
    stop_words_(MakeUniqueNonEmptyStrings(SplitIntoWords(stor_stop_words))) {
    using namespace std;
    if (!all_of(stop_words_.begin(), stop_words_.end(),
        [](string_view word) {
            return IsValidWord(word);
        }))
    {
        throw invalid_argument("Some of stop words are invalid"s);
    }
}

SearchServer::SearchServer(const string_view& stop_words_text)
    : stor_stop_words(static_cast<string>(stop_words_text)), stop_words_(MakeUniqueNonEmptyStrings(SplitIntoWords(stor_stop_words))) {
    using namespace std;
    if (!all_of(stop_words_.begin(), stop_words_.end(),
        [](string_view word) {
            return IsValidWord(word);
        }))
    {
        throw invalid_argument("Some of stop words are invalid"s);
    }
}

void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    stor_documents.push_back(static_cast<string>(document));
    const vector<string_view> words = SplitIntoWordsNoStop(stor_documents.back());

    const double inv_word_count = 1.0 / words.size();
    for (const string_view& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

vector<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

vector<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static const map<string_view, double> empty_result;
    if (document_to_word_freqs_.count(document_id) == 0) {
        return empty_result;
    }
    return document_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
    auto it = find(document_ids_.begin(), document_ids_.end(), document_id);
    if (it == document_ids_.end()) {
        return;
    }
    document_ids_.erase(it);
    documents_.erase(document_id);
    for (auto& [word, id_freq] : SearchServer::word_to_document_freqs_) {
        id_freq.erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query, int document_id) const {
    if (find(document_ids_.begin(), document_ids_.end(), document_id) == document_ids_.end()) {
        using namespace std::literals::string_literals;
        throw std::out_of_range("incorrect document id"s);
    }

    const auto query = SearchServer::ParseQuery(raw_query);

    vector<string_view> matched_words;
    for (const string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            return { matched_words, documents_.at(document_id).status };
        }
    }
    for (const string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    
    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

bool SearchServer::IsValidWord(string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (! (IsValidWord(word))) {
            throw invalid_argument("Word "s + static_cast<string>(word) + " is invalid"s);
        }
        if (!SearchServer::IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return accumulate(ratings.begin(), ratings.end(), 0)
                    / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text.front() == '-') {
        is_minus = true;
        text.remove_prefix(1);
    }

    if (text.empty() || text[0] == '-' || !SearchServer::IsValidWord(text)) {
        throw invalid_argument("Query word "s + static_cast<string>(text) + " is invalid");
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const string_view& text) const {
    Query result;
    for (string_view& word : SplitIntoWords(text)) {
        const auto query_word = SearchServer::ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const {
    return log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}