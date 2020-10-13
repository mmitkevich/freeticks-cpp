#pragma once
#include "ft/utils/Common.hpp"

namespace ft::utils {

/// associate function with string name and call it lazily
template<typename FnT>
struct IdFn {
    std::string_view id;
    FnT fn;

    constexpr IdFn(std::string_view id, FnT fn)
    : id(id)
    , fn(fn)
    {}
    template<typename...ArgsT>
    auto operator()(ArgsT...args) {
        return fn(std::forward<ArgsT>(args)...);
    }
};

namespace detail {
    template<class InterfaceT,  template<typename ImplT> typename AdapterT, typename... FactoriesT>
    class UniquePtr {
    public:
        template<typename...ArgsT>
        UniquePtr(ArgsT...args)
        : values_(std::forward<ArgsT>(args)...)
        {}

        template<typename...ArgsT>
        auto operator()(std::string_view id, ArgsT...args) {
            bool done = false;
            std::unique_ptr<InterfaceT> result;
            mp::tuple_for_each(values_, [&](auto &factory) {
                if(!done && factory.id==id) {
                    done = true;
                    auto valptr = factory(std::forward<ArgsT>(args)...);
                    result =  std::unique_ptr<InterfaceT>(new AdapterT(std::move(valptr)));
                }
            });
            if(!done) {
                std::ostringstream ss;
                ss<<"factory does not support '"<<id<<"'";
                throw std::runtime_error(ss.str());
            }
            return result;
        }    
    private:
        std::tuple<FactoriesT...> values_;
    };
}
template<class InterfaceT, template<typename ImplT> typename AdapterT>
class Factory {
public:
    using Interface = InterfaceT;
public:
    template<typename...FactoriesT>
    static auto unique_ptr(FactoriesT&&... args) { return detail::UniquePtr<Interface, AdapterT, FactoriesT...> {args...}; }
};
} // ft::utils