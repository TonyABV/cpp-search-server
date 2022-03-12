#include "string_processing.h"

#include <utility>

using namespace std;

vector<string_view> SplitIntoWords(const string_view& str_text) {
    vector<string_view> result;
    string_view text = str_text;
    const int64_t pos_end = text.npos;
    while (true) {
        int64_t space = text.find(' ', 0);

        result.push_back(space == pos_end ? text : text.substr(0, space));
        if (space == pos_end) {
            break;
        }
        else {
            text.remove_prefix(space + 1);
        }
    }
    return result;
}