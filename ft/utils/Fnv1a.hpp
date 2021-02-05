#pragma once

#include <cstdint>
#include <ostream>
#include <functional>

namespace ft {inline namespace util {

// FNV-1a 32bit compile time hashing algorithm.
constexpr std::size_t fnv1a_32(char const* s, std::size_t count)
{
    return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
}

inline int constexpr ct_strlen(const char* str)
{
    return *str ? 1 + ct_strlen(str + 1) : 0;
}

class Fnv1a
{
public:
    constexpr Fnv1a() : hash_(0) {}
    constexpr Fnv1a(std::size_t hash) : hash_(hash) {}
   
   /*
    Fnv1a(const Fnv1a &rhs) = default;
    Fnv1a& operator=(const Fnv1a &rhs) = default;
    
    Fnv1a(Fnv1a &&rhs) = default;
    Fnv1a& operator=(Fnv1a &&rhs) = default;
    
    ~Fnv1a() { } */
    
    Fnv1a(std::string s)
    : hash_(fnv1a_32(s.c_str(), s.size())) {}

    constexpr Fnv1a(const char* s)
    : hash_(fnv1a_32(s, ct_strlen(s))) {}
    
    constexpr Fnv1a(const char* s, std::size_t len)
    : hash_(fnv1a_32(s, len)) {}
    

    std::size_t hash_value() const {
        return hash_;
    }

    bool operator==(const Fnv1a &rhs) const {
        return hash_==rhs.hash_;
    }

    bool empty() const {
        return hash_==0;
    }
private:
    std::size_t hash_;
};


inline std::ostream& operator<<(std::ostream &os, const Fnv1a &rhs) {
    os << "0x" << std::hex << rhs.hash_value() << "_h";
    return os;
}

}} //ft::util

namespace std {
    template<> 
    struct hash<ft::util::Fnv1a> {
        std::size_t operator()(const ft::util::Fnv1a& val) const noexcept {
            return val.hash_value();
        }
    };
}