#pragma once
#include "ft/utils/Common.hpp"
#include <iomanip>
#include <iostream>

#include <ft/capi/ft-types.h>

namespace ft::core {

template<typename DerivedT>
class Hashable {};

class Identifier : public ft_id_t, public Hashable<Identifier>
{   
    using Base = ft_id_t;
public:
    Identifier(const ft_id_t &id) noexcept
    : Base(id) {}

    Identifier() noexcept
    : Base{0, 0}
    {}
    Identifier(std::uint64_t low) noexcept
    : Base{low, 0} 
    {}
    Identifier(std::uint64_t low, std::uint64_t high) noexcept
    : Base{low, high}
    {}
    
    bool operator==(const Identifier& rhs) const noexcept { return low==rhs.low && high==rhs.high; }
    bool operator!=(const Identifier& rhs) const noexcept { return low!=rhs.low || high!=rhs.high; }
    bool operator<(const Identifier& rhs) const noexcept { return high<rhs.high || high==rhs.high && low<rhs.low; }

    const uint8_t* data() const noexcept { return reinterpret_cast<const uint8_t*>(this); }
    constexpr std::size_t size() const noexcept { return 16; }
    constexpr std::size_t length() const noexcept { return 16; }

    std::size_t hash_value() const noexcept {
        return low ^ high;
    }
    bool empty() const noexcept { return low==0 && high==0; }
};


inline std::ostream& operator<<(std::ostream& os, const Identifier& id) {
    
    std::ios_base::fmtflags f(os.flags());
    /*const uint8_t *b = id.data();
    os << std::hex << std::setfill('0')
        << std::setw(2) << (int)b[0] << std::setw(2) << (int)b[1] << std::setw(2) << (int)b[2] << std::setw(2) << (int)b[3]
        << '-' << std::setw(2) << (int)b[4] << std::setw(2) << (int)b[5]
        << '-' << std::setw(2) << (int)b[6] << std::setw(2) << (int)b[7] << '-' << std::setw(2) << (int)b[8] << std::setw(2) << (int)b[9]
        << '-' << std::setw(2) << (int)b[10] << std::setw(2) << (int)b[11] << std::setw(2) << (int)b[12] << std::setw(2) << (int)b[13] << std::setw(2) << (int)b[14] << std::setw(2) << (int)b[15];
    */
    os << std::hex << "0x";
    if(id.high)
        os << id.high;
    os << id.low;
    os.flags(f);
    return os;
}

using VenueInstrumentId = Identifier;
using InstrumentId = Identifier;

class Identifiable {
public:
    Identifiable() noexcept = default;
    
    Identifiable(Identifier val) noexcept
    : id_(val) {}

    // unique 
    Identifier id() const noexcept { return id_; }
    void id(Identifier id) noexcept { id_ = id; }
protected:
    Identifier id_ {};
};

}

namespace std {
    template<>
    struct hash<ft::core::Identifier> {
        hash() = default;
        hash(const hash&) = default;
        hash(hash&&) = default;
        std::size_t operator()(const ft::core::Identifier& val) const noexcept {
            return val.hash_value();
        }
    };
};