// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Jeremy Borgman

#include "strfparse.h"

#include <algorithm>
#include <ctime>
#include <iterator>
#include <map>
#include <sstream>

namespace strfparse {
std::vector<std::string> split(std::string const& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<char> create_specifier_list()
{

    std::vector<char> allowed_specifier{ 'Y', 'H', 'a', 'A', 'b', 'B', 'c', 'C',
                                         'd', 'D', 'e', 'F', 'g', 'G', 'h', 'H',
                                         'I', 'j', 'm', 'M', 'n', 'p', 'r', 'R',
                                         'S', 't', 'T', 'u', 'U', 'V', 'w', 'W',
                                         'x', 'X', 'y', 'Y', 'z', 'Z' };
    return allowed_specifier;
}

std::string replace_all(std::string input,
                        std::string const& to_find,
                        std::string const& to_replace)
{
    size_t pos = 0;
    while ((pos = input.find(to_find, pos)) != std::string::npos) {
        input.replace(pos, to_find.length(), to_replace);
        pos += to_replace.length();
    }

    return input;
}

std::vector<char> match_specifiers(std::string const& specifier,
                                   std::vector<char> allowed_specifier)
{
    std::vector<char> spec_list;
    if (specifier.size() < 2) {
        return spec_list;
    }

    for (size_t i = 0; i < specifier.size() - 1; i++) {
        if (specifier[i] == '%') {
            spec_list.push_back(specifier[i + 1]);
        }
    }

    std::sort(spec_list.begin(), spec_list.end());
    spec_list.erase(std::unique(spec_list.begin(), spec_list.end()),
                    spec_list.end());
    std::sort(allowed_specifier.begin(), allowed_specifier.end());
    allowed_specifier.erase(
      std::unique(allowed_specifier.begin(), allowed_specifier.end()),
      allowed_specifier.end());

    std::vector<char> overlap;
    std::set_intersection(spec_list.begin(),
                          spec_list.end(),
                          allowed_specifier.begin(),
                          allowed_specifier.end(),
                          back_inserter(overlap));

    return overlap;
}

std::string format_time_string(std::string const& specifier)
{
    if (specifier.empty()) {
        return "";
    }

    auto allowed_specifier = create_specifier_list();
    auto overlap = match_specifiers(specifier, allowed_specifier);
    if (overlap.empty()) {
        return specifier;
    }

    const std::time_t t = std::time(nullptr);
    std::tm tmBuf{};
#if defined(_WIN32)
    if (localtime_s(&tmBuf, &t) != 0) {
        return specifier;
    }
#else
    if (localtime_r(&t, &tmBuf) == nullptr) {
        return specifier;
    }
#endif

    // Prefix with a sentinel character so strftime returns >= 1 even for
    // specifiers that expand to an empty string (e.g. %p in 24h locales),
    // distinguishing valid empty output from buffer exhaustion (0).
    std::map<char, std::string> lookup_table;
    for (const char e : overlap) {
        const char fmt[4] = { 'x', '%', e, '\0' };
        char buff[256] = {};
        const std::size_t n = std::strftime(buff, sizeof(buff), fmt, &tmBuf);
        if (n > 0) {
            lookup_table.emplace(e, std::string(buff + 1, n - 1));
        }
    }

    // Sub into original string
    std::string delim = "%";
    auto output_string = specifier;
    for (auto const& row : lookup_table) {
        auto to_find = delim + row.first;
        output_string = replace_all(output_string, to_find, row.second);
    }
    return output_string;
}
}
