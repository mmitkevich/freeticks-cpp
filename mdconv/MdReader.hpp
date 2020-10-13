#pragma once
#include "ft/utils/Common.hpp"
#include <chrono>
#include <sstream>
#include "ft/core/MdGateway.hpp"
#include "toolbox/sys/Time.hpp"

namespace ft {
namespace tbs = toolbox::sys;

template<typename GatewayT>
class MdReader
{
public:
    MdReader(GatewayT& gateway)
    : gateway_(gateway)
    {}
    GatewayT& gateway() { return gateway_; }
    void on_state_changed(core::GatewayState state, core::GatewayState old_state, core::ExceptionPtr err) {
        if(state==core::GatewayState::Failed && err)
            std::cerr << "error: "<<err->what();
        if(state==core::GatewayState::Stopped) {
            gateway().report(std::cerr);
            auto elapsed = tbs::MonoClock::now() - start_timestamp_;
            auto total_received = gateway().stats().total_received;
            TOOLBOX_INFO << " read " << total_received
                << " in " << elapsed.count()/1e9 <<" s" 
                << " at "<< (1e3*total_received/elapsed.count()) << " mio/s";
        }
    };

    template<typename OptionsT>
    void operator()(std::string_view input, const OptionsT &opts) {
        start_timestamp_ = tbs::MonoClock::now();
        gateway().url(input);            
        gateway().filter(opts.filter);
        gateway().state_changed().connect(tbu::bind<&MdReader::on_state_changed>(this));
        gateway().start();
        gateway().stop();
    }
private:
    GatewayT& gateway_;
    tb::MonoTime start_timestamp_;
};

} // ft::md