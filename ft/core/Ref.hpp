#pragma once

#include <type_traits>
#include <memory>

namespace ft { inline namespace core {

/// type-erased reference
template<class IfaceT, template<class > class IfacePtrTT=std::add_pointer>
class BasicRef {
public:
    using Iface = IfaceT;
    using IfacePtr = IfacePtrTT<Iface>;
    BasicRef(IfacePtr impl)
    : impl_(std::move(impl)) {}
    
    template<class I, template<class> class P=IfacePtrTT>
    BasicRef<I, P> as() {
        return BasicRef<I, P>(impl_);
    }

    template<class I, template<class> class P=IfacePtrTT>
    BasicRef<I,P> to() {
        return BasicRef<I, P>(std::move(impl_));
    }
protected:
    IfacePtr impl_;
};

}} // ft::core