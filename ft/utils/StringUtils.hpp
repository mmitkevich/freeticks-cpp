#pragma once
#include <string>
#include <boost/algorithm/hex.hpp> 
#include <toolbox/Config.h>

namespace ft { inline namespace util {

template<typename BufferT>
std::string to_hex(BufferT buf) {
    std::string result(buf.size()*2,'\0');
    boost::algorithm::hex((const char*)buf.data(), ((const char*)buf.data())+buf.size(), result.begin());
    return result;
}

std::string TOOLBOX_API to_hex_dump(std::string_view buf);

std::wstring to_wstring(std::string_view sv);

inline std::string_view str_suffix(std::string_view s, char sep) {
    auto pos = s.rfind(sep);
    if(pos==std::string::npos)
        return {};
    return {s.data()+pos + 1, s.size() - pos - 1};
}
inline std::string_view str_prefix(std::string_view s, char sep) {
    auto pos = s.find(sep);
    if(pos==std::string::npos)
        return {};
    return {s.data(), pos};
}

}} //ft::util