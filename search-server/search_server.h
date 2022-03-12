#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <execution>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view& stop_words_text);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    template <typename DocumentPredicate, class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const;
    template <class ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const;

    size_t GetDocumentCount() const;

    std::vector<int>::iterator begin();

    std::vector<int>::iterator end();

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template<class ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;
    template<class ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, 
        const std::string_view& raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const std::string stor_stop_words;
    std::list<std::string> stor_documents;
    const std::set<std::string_view> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;

    bool IsStopWord(const std::string_view& word) const;

    static bool IsValidWord(const std::string& word);
    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord( std::string_view& text) const;

    struct Query {
        std::set<std::string_view, std::less<>> plus_words;
        std::set<std::string_view, std::less<>> minus_words;
    };

    struct vec_Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view& text) const;

    template <class ExecutionPolicy>
    vec_Query ParseQuery(ExecutionPolicy&& policy, const std::string_view& text) const;

    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate, class ExecutionPolicy>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const vec_Query& query, DocumentPredicate document_predicate) const;/**/
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    using namespace std;
    if (!all_of(stop_words_.begin(), stop_words_.end(),
        [](string_view word) {
            return IsValidWord(static_cast<string>(word));
        }))
    {
        throw invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);
    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate, class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const {

    vec_Query query = ParseQuery(policy, raw_query);

    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
        std::sort(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(std::unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());

        std::sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(
            std::unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());
    }

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const {
        return SearchServer::FindTopDocuments(policy, raw_query, [&status](int document_id, DocumentStatus document_status, int rating)
        {return document_status == status;});
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const {
    return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate, class ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const vec_Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(8);

    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(),
        [&document_to_relevance, &document_predicate, this](const std::string_view& word) {
            if (word_to_document_freqs_.count(word) != 0) {            
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });

    std::for_each(policy, query.minus_words.begin(), query.minus_words.end(),
        [this, &document_to_relevance](const std::string_view& word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.erase(document_id);
                }
            }
        });

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }

    return matched_documents;
}

template<class ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutionPolicy&& policy, const std::string_view& raw_query, int document_id) const {
    if (!documents_.count(document_id)) {
        using namespace std::literals::string_literals;
        throw std::out_of_range("incorrect document id"s);
    }

    const SearchServer::vec_Query query = std::move(SearchServer::ParseQuery(policy, raw_query));
    std::vector<std::string_view> matched_words;

    if (any_of(make_move_iterator(query.minus_words.begin()),
        make_move_iterator(query.minus_words.end()),
        [this, &document_id](const std::string_view& minus_word) {
            return (word_to_document_freqs_.at(minus_word).count(document_id));
        })) {
        return { matched_words, documents_.at(document_id).status };
    }

        copy_if(make_move_iterator(query.plus_words.begin()),
            make_move_iterator(query.plus_words.end()),
            std::back_inserter(matched_words),
            [this, &document_id](const std::string_view& word) {
                return word_to_document_freqs_.at(word).count(document_id);
            });

    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
        sort(policy, matched_words.begin(), matched_words.end());
        auto last = unique(matched_words.begin(), matched_words.end());
        return { {matched_words.begin(), last}, documents_.at(document_id).status };
    }

    return { {matched_words}, documents_.at(document_id).status };
}

template<class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    using namespace std;
    auto it = find(document_ids_.begin(), document_ids_.end(), document_id);
    if (it == document_ids_.end())
    {
        return;
    }
    document_ids_.erase(it);
    documents_.erase(document_id);

    std::map<std::string_view, double>* words_freqs = &document_to_word_freqs_[document_id];
    vector<string> to_remove(words_freqs->size());

    transform(policy, words_freqs->begin(), words_freqs->end(), to_remove.begin(),
        [](auto& word_freq) {
            return std::move(word_freq.first);
        });
    for_each(policy, to_remove.begin(), to_remove.end(),
        [this, &document_id](string& word) {
            word_to_document_freqs_[std::move(word)].erase(document_id);
        });
    document_to_word_freqs_.erase(document_id);
}

template <class ExecutionPolicy>
SearchServer::vec_Query SearchServer::ParseQuery(ExecutionPolicy&& policy, const std::string_view& text) const{
    vec_Query result;
    for (std::string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(std::move(query_word.data));
            }
            else {
                result.plus_words.push_back(std::move(query_word.data));
            }
        }
    }
    if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::sequenced_policy>) {
        sort(result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(
            std::unique(result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());

        std::sort(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(
            std::unique(result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());
    }
    
    return result;
}