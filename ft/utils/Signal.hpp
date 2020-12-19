#pragma once

#include "ft/utils/Common.hpp"
#include "toolbox/util/Slot.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>

namespace mp = boost::mp11;

namespace tb = toolbox;

namespace ft { inline namespace util {


template<typename T, typename ...ArgsT> 
struct Signal : public tb::Signal<T, ArgsT...> {
    using Base = tb::Signal<T, ArgsT...>;
    using value_type = T;
    using Base::Base;
    using Base::operator();
    using Base::operator bool;
    using Base::connect;
    using Base::disconnect;
};

template<typename TypeListT, std::size_t size=1>
struct SignalTuple 
{
    template<typename...Args>
    SignalTuple(Args...args)
    : impl_(std::forward<Args>(args)...) {}

    template<typename T>
    void connect(tb::BasicSlot<T> fn)
    {
        std::get< mp::mp_find<TypeListT, T>::value >(impl_).connect(fn);
    }
    
    template<typename T>
    void disconnect(tb::BasicSlot<T> fn)
    {
        std::get< mp::mp_find<TypeListT, T>::value >(impl_).disconnect(fn);
    }    

    template<typename Fn>
    void for_each(Fn&& fn) {
        mp::tuple_for_each(impl_, fn);
    }
    mp::mp_rename<mp::mp_transform<Signal, TypeListT>, std::tuple> impl_;
};

}} // ft::util