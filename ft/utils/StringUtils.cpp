#include "StringUtils.hpp"
#include <locale>

namespace ft { inline namespace util {


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

std::string to_hex_dump(std::string_view buf) {
    std::stringstream ss;
    static char hex[]="0123456789ABCDEF";
    const char* b = (const char*)buf.data();
    for(std::size_t i=0; i<buf.size(); i++) {
        char c = buf[i];
        ss << hex[(c>>4)&0xF] << hex[c&0x0F] << ' ';
    }
    ss << std::endl;
    for(std::size_t i=0; i<buf.size(); i++) {
        char c = buf[i];
        if(c<' ')
            c=' ';
        ss << ' ' << c << ' ';
    }
    return ss.str();
}

}} //ft::util