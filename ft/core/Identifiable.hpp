#pragma once
#include "ft/utils/Common.hpp"


namespace ft::core {


using Identifier = std::uint64_t;
using VenueInstrumentId = Identifier;

class Identifiable {
public:
    // unique 
    Identifier id() { return id_; }
    void id(Identifier id) { id_ = id; }
protected:
    Identifier id_ {};
};



}