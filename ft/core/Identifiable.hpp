#pragma once
#include "ft/utils/Common.hpp"
#include <iomanip>
#include <iostream>
#include <random>
#include <ft/capi/ft-types.h>
#include "ft/utils/Fnv1a.hpp"

namespace ft { inline namespace core {

template<typename DerivedT>
class Hashable {};

class Identifier : public ft_id_t, public Hashable<Identifier>
{   
    using Base = ft_id_t;
public:
    Identifier(const ft_id_t &id) noexcept
    : Base(id)
    {}
    Identifier() noexcept
    : Base{0, 0}
    {}
    explicit Identifier(std::uint64_t low) noexcept
    : Base{low, 0} 
    {}
    explicit Identifier(std::uint64_t low, std::uint64_t high) noexcept
    : Base{low, high}
    {}
    
    class Generator {
      public:
        Identifier operator()() {
            return Identifier {distrib_(gen_), distrib_(gen_)};
        }
      protected:
        std::random_device rd_;
        std::mt19937 gen_{rd_()};
        std::uniform_int_distribution<uint64_t> distrib_;
    };

    auto& low(uint64_t val) {
        Base::low = val;
        return *this;
    }
    
    auto& high(uint64_t val) {
        Base::high = val;
        return *this;
    }

    auto low() const { return Base::low; }
    auto high() const { return Base::high; }

    bool operator==(const Identifier& rhs) const noexcept { return low()==rhs.low() && high()==rhs.high(); }
    bool operator!=(const Identifier& rhs) const noexcept { return low()!=rhs.low() || high()!=rhs.high(); }
    bool operator<(const Identifier& rhs) const noexcept { return high()<rhs.high() || high()==rhs.high() && low()<rhs.low(); }

    Identifier operator^(const Identifier& rhs) {
        return Identifier {low()^rhs.low(), high()^rhs.high()} ;
    }

    const uint8_t* data() const noexcept { return reinterpret_cast<const uint8_t*>(this); }
    constexpr std::size_t size() const noexcept { return 16; }
    constexpr std::size_t bytesize() const noexcept { return 16; }

    operator bool() const { return !empty(); }

    std::size_t hash_value() const noexcept {
        return low() ^ high();
    }
    bool empty() const noexcept { return low()==0 && high()==0; }
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
    if(id.high())
        os << id.high();
    os << id.low();
    os.flags(f);
    return os;
}

using VenueInstrumentId = Identifier;
using InstrumentId = Identifier;
using PeerId = Identifier;

class Identifiable {
public:
    Identifiable() noexcept = default;
    
    Identifiable(Identifier val) noexcept
    : id_(val) {}
    Identifiable(Identifiable* parent, Identifier id) {
        id_ = id.empty() ? make_random_id(parent) : id;
    }

    static Identifier make_random_id(Identifiable* parent=nullptr) {
        auto id =  Identifier::Generator{}();
        if(parent)
            id = id ^ parent->id();
        return id;
    }
    // unique 
    Identifier id() const noexcept { return id_; }
    void id(Identifier id) noexcept { id_ = id; }
protected:
    Identifier id_ {};
};


}} // ft::core

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

}