#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string_view> SplitIntoWords(const std::string_view& str_text);

template <typename StringContainer>
std::set<std::string_view> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string_view> non_empty_strings;
    for (const std::string_view& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}
