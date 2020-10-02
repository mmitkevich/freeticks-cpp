#pragma once

#include "toolbox/util/Slot.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>

namespace ft::utils {

namespace tbu = toolbox::util;
namespace mp = boost::mp11;

template<typename MessageT> 
struct Signal : public tbu::Signal<const MessageT&> {
    using Base = tbu::Signal<const MessageT&>;
    using Message = MessageT;
    using Base::Base;
    using Base::operator();
    using Base::operator bool;
    using Base::connect;
    using Base::disconnect;
};

template<typename TypeListT>
struct SignalTuple 
{
    template<typename...Args>
    SignalTuple(Args...args)
    : impl_(std::forward<Args>(args)...) {}

    template<typename T>
    void connect(tbu::BasicSlot<const T&> fn)
    {
        std::get< mp::mp_find<TypeListT, T>::value >(impl_).connect(fn);
    }
    
    template<typename T>
    void disconnect(tbu::BasicSlot<const T&> fn)
    {
        std::get< mp::mp_find<TypeListT, T>::value >(impl_).disconnect(fn);
    }    

    template<typename Fn>
    void for_each(Fn&& fn) {
        mp::tuple_for_each(impl_, fn);
    }
    mp::mp_rename<mp::mp_transform<Signal, TypeListT>, std::tuple> impl_;
};

}