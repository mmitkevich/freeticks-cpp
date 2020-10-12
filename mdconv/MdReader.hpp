#pragma once
#include "ft/utils/Common.hpp"
#include <chrono>
#include <sstream>

namespace ft {

template<typename GatewayT>
class MdReader
{
public:
    MdReader(GatewayT& gateway)
    : gateway_(gateway)
    {}
    GatewayT& gateway() { return gateway_; }

    template<typename OptionsT>
    void operator()(std::string_view input, const OptionsT &opts) {
        auto start_ts = std::chrono::system_clock::now();
        auto finally = [&] {
            gateway().report(std::cerr);
            auto elapsed = std::chrono::system_clock::now() - start_ts;
            auto total_received = gateway().stats().total_received;
            TOOLBOX_INFO << " read " << total_received
                << " in " << elapsed.count()/1e9 <<" s" 
                << " at "<< (1e3*total_received/elapsed.count()) << " mio/s";
        };
        try {
            gateway().url(input);            
            gateway().filter(opts.filter);
            gateway().run();
            finally();            
        }catch(std::runtime_error &e) {
            finally();
            TOOLBOX_INFO << "'" << gateway().url() << "' " << e.what();
            throw;
        }
    }
private:
    GatewayT& gateway_;
};

} // ft::md