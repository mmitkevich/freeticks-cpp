#pragma once
#include "ft/utils/Common.hpp"

namespace ft::utils {

template<typename ImplT>
struct HeapAllocated {
    std::string id;
    HeapAllocated(std::string id)
    : id(id){ }
    template<typename...ArgsT>
    std::unique_ptr<ImplT> operator()(ArgsT...args) {
        return std::make_unique<ImplT>(std::forward<ArgsT>(args)...);
    }
};

template<class InterfaceT, template<typename ImplT> typename AdapterT>
class Factory {
public:
    using Interface = InterfaceT;
   
    template<typename... FactoriesT>
    class MultipleUniqueFactory {
    public:
        template<typename...ArgsT>
        MultipleUniqueFactory(ArgsT...args)
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
public:
    template<typename...FactoriesT>
    static MultipleUniqueFactory<FactoriesT...> of(FactoriesT&&... args) { return MultipleUniqueFactory<FactoriesT...> {args...}; }
};
} // ft::utils