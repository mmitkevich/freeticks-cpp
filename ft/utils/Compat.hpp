#pragma once

#include <functional>

// C++ 20 polyfills for c++ 17

namespace ft { inline namespace util {

template <class T>
struct unwrap_reference { using type = T; };
template <class U>
struct unwrap_reference<std::reference_wrapper<U>> { using type = U&; };
 
template<class U>
using unwrap_reference_t = typename unwrap_reference<U>::type;

template< class T >
struct unwrap_ref_decay : unwrap_reference<std::decay_t<T>> {};

template<class U>
using unwrap_ref_decay_t = typename unwrap_ref_decay<U>::type;

} } // ft::std
