#pragma once
#include "ft/utils/Common.hpp"
#include <chrono>

namespace ft {


template<typename GatewayT>
class MdReader
{
public:
    MdReader(std::string_view id, std::unique_ptr<GatewayT> gateway)
    : id_(id)
    , gateway_(std::move(gateway))
    {}

    GatewayT &gateway() { return *gateway_; }

    bool match(std::string_view id) const {
        return id_ == id;
    }
    
    const std::string_view id() const {
        return id_;
    }

    template<typename OptionsT>
    void run(std::string_view input, const OptionsT &opts) {
        auto start_ts = std::chrono::system_clock::now();
        auto finally = [&] {
            gateway().report(std::cerr);
            auto elapsed = std::chrono::system_clock::now() - start_ts;
            auto total_received = gateway().stats().total_received;
            TOOLBOX_INFO << id() << " read " << total_received
                << " in " << elapsed.count()/1e9 <<" s" 
                << " at "<< (1e3*total_received/elapsed.count()) << " mio/s";
        };
        try {
            gateway().input(input);            
            gateway().filter(opts.filter);
            gateway().run();
            finally();            
        }catch(std::runtime_error &e) {
            finally();
            TOOLBOX_INFO << "'" << gateway().input() << "' " << e.what();
            throw;
        }
    }
private:
    std::unique_ptr<GatewayT> gateway_;
    std::string id_;
};


template<typename...ImplsT>
class MdReaders {
public:
    template<typename...ArgsT>
    MdReaders(ArgsT... args)
    : impls_(std::forward<ArgsT>(args)...)
    {}

    template<typename OptionsT>
    void run(std::string_view id, const OptionsT& opts)
    {
        for(auto& input: opts.inputs) {
            bool done = false;
            mp::tuple_for_each(impls_, [&](auto &e) {
                if(!done && e.match(id)) {
                    done = true;
                    try {
                        e.run(input, opts);
                    }catch(std::runtime_error &e) {
                        TOOLBOX_INFO << "'" << input << "' " << e.what();
                    }
                }
            });
        }
    }
private:
    std::tuple<ImplsT...> impls_;
};

template<typename...ArgsT>
auto make_md_readers(ArgsT...args) { return MdReaders<ArgsT...>(std::forward<ArgsT>(args)...); }

} // ft::md