
#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> remove_these_id;
    set<set<string_view>> unique_words;
    for (const int& document_id : search_server) {
        const map<string_view, double>& doc = search_server.GetWordFrequencies(document_id);
        set<string_view> words;
        for (auto& [word, freq] : doc) {
            words.insert(word);
        }
        if (unique_words.find(words) == unique_words.end()) {
            unique_words.insert(words);
        }
        else {
            remove_these_id.insert(document_id);
        }
    }
    for (const auto& id : remove_these_id) {
        search_server.RemoveDocument(id);
        cout << "Found duplicate document id " << id << endl;
    }
}
