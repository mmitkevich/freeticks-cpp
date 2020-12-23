#pragma once
#include "ft/core/Identifiable.hpp"
#include "ft/core/Parameters.hpp"

namespace ft { inline namespace core {

template<typename DerivedT>
class BasicObject : public Identifiable, public BasicParameterized<DerivedT>
{
    using Identifiable::Identifiable;
};

}}