#pragma once
#include <string>
#include <boost/algorithm/hex.hpp> 

namespace ft::utils {

std::string to_hex(std::string_view sv) {
    std::string result(sv.size()*2,'\0');
    boost::algorithm::hex(sv.begin(), sv.end(), result.begin());
    return result;
}

}