#pragma once
#include <string>

namespace ft { inline namespace util {

std::string to_hex(std::string_view sv);
std::wstring to_wstring(std::string_view sv);

inline std::string_view str_suffix(std::string_view s, char sep) {
    auto pos = s.rfind(sep);
    if(pos==std::string::npos)
        return {};
    return {s.data()+pos + 1, s.size() - pos - 1};
}

}} //ft::util