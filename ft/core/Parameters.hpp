#pragma once
#include "ft/utils/Common.hpp"
#include "toolbox/util/Json.hpp"

namespace ft { inline namespace core {

using MutableParameters = tb::json::MutableDocument;
using Parameters = tb::json::MutableElement;
using ParametersSignal = tb::Signal<const Parameters&>;

class IParameterized {
public:
    virtual ~IParameterized() = default;
    /// update parameters. when replace==true, old parameters are cleared
    virtual void parameters(const Parameters& parameters, bool replace=false) = 0;
    /// retrieve current parameters
    virtual const Parameters& parameters() const = 0;
};

template<class Self, class Base=IParameterized>
class ParameterizedImpl : virtual public Base  {
    auto* impl() { return static_cast<Self*>(this)->impl(); }
    const auto* impl() const { return static_cast<const Self*>(this)->impl(); }
public:

    void parameters(const Parameters& parameters, bool replace=false) override { 
        impl()->parameters(parameters, replace);
    }
    const Parameters& parameters() const override { return impl()->parameters(); }
};

template<class Self>
class BasicParameterized {
public:
    /// update parameters. when replace==true, old parameters are cleared
    void parameters(const Parameters& parameters, bool replace=false) { 
        // copy on write
        MutableParameters::copy(parameters, parameters_);
        static_cast<Self*>(this)->on_parameters_updated(parameters_);
    }
    void clear() {
        parameters_.clear();
    }
    void on_parameters_updated(const core::Parameters& params) {}
    /// retrieve current parameters
    const Parameters& parameters() const { return parameters_; }
protected:
    Parameters& parameters() { return parameters_; }
    MutableParameters parameters_;  // size is two sizeof(vector)
};

}} // ft::core