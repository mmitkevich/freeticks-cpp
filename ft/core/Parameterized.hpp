#pragma once
#include "ft/utils/Common.hpp"
#include "toolbox/util/Json.hpp"

namespace ft::core {

using MutableParameters = toolbox::json::MutableDocument;
using Parameters = toolbox::json::MutableElement;
using ParametersSignal = tbu::Signal<const Parameters&>;

class IParameterized {
public:
    virtual ~IParameterized() = default;
    /// update parameters. when replace==true, old parameters are cleared
    virtual void parameters(const Parameters& parameters, bool replace=false) = 0;
    /// retrieve current parameters
    virtual const Parameters& parameters() const = 0;
    /// parameters changed notification: source, new parameters
    virtual ParametersSignal& parameters_updated() = 0;
};



template<class BaseT = IParameterized>
class BasicParameterized : public BaseT {
public:
    /// update parameters. when replace==true, old parameters are cleared
    void parameters(const Parameters& parameters, bool replace=false) { 
        // FIXME: merge?
        MutableParameters::copy(parameters, parameters_);
        parameters_updated_.invoke(parameters_);
    }
    /// retrieve current parameters
    const Parameters& parameters() const { return parameters_; }
    /// parameters changed notification: source, new parameters
    ParametersSignal& parameters_updated() { return parameters_updated_; }

protected:
    ParametersSignal parameters_updated_;
    MutableParameters parameters_;
};

};