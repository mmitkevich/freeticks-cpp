#include "StringUtils.hpp"
#include <locale>
#include <boost/algorithm/hex.hpp> 

namespace ft { inline namespace util {

std::string to_hex(std::string_view sv) {
    std::string result(sv.size()*2,'\0');
    boost::algorithm::hex(sv.begin(), sv.end(), result.begin());
    return result;
}

template<class Facet>
struct deletable_facet : Facet
{
    template<class ...Args>
    deletable_facet(Args&& ...args) : Facet(std::forward<Args>(args)...) {}
    ~deletable_facet() {}
};

std::wstring to_wstring(std::string_view sv) {
    static thread_local std::wstring_convert<
        deletable_facet<std::codecvt<wchar_t, char, std::mbstate_t>>
        , wchar_t> conv;
    std::wstring wstr = conv.from_bytes(sv.data());
    return wstr;
}

}} //ft::util