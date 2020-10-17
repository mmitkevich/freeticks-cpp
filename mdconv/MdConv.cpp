#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <boost/mp11.hpp>

#include "ft/core/Instrument.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/MdGateway.hpp"
#include "ft/core/Parameterized.hpp"
#include "toolbox/net/Pcap.hpp"
#include "toolbox/util/Finally.hpp"
#include "toolbox/util/Json.hpp"
#include "toolbox/util/Options.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/net/EndpointFilter.hpp"

#include "ft/pcap/PcapMdGateway.hpp"
#include "ft/qsh/QshMdGateway.hpp"

#include "ft/spb/SpbProtocol.hpp"

#include "ft/utils/Factory.hpp"
#include "toolbox/util/Json.hpp"

namespace ft {

namespace tbu = toolbox::util;
namespace tbs = toolbox::sys;
namespace tb = toolbox;
namespace tbj = toolbox::json;

//void handler(const ft::api::TickMessage& e) {
//    std::cout << e << std::endl;
//}

class MdConv {
public:
    using This = MdConv;
    struct MdOptions {
        int log_level = 0;
        std::string input_format;
        std::string output_format;
        std::vector<std::string> inputs;
        tb::optional<std::string> output;
        toolbox::EndpointsFilter filter;
        int max_packet_count{0};
    };
public:
    core::IMdGateway& gateway() { return *gateway_; }

    void on_state_changed(core::GatewayState state, core::GatewayState old_state, core::ExceptionPtr err) {
        if(state==core::GatewayState::Failed && err)
            std::cerr << "error: "<<err->what();
        if(state==core::GatewayState::Stopped) {
            //gateway().report(std::cerr);
            auto elapsed = tbs::MonoClock::now() - start_timestamp_;
            auto total_received = gateway().stats().total_received;
            TOOLBOX_INFO << " read " << total_received
                << " in " << elapsed.count()/1e9 <<" s" 
                << " at "<< (1e3*total_received/elapsed.count()) << " mio/s";
        }
    };

    void on_tick(const core::Tick& e) {
        //TOOLBOX_INFO << e;
        auto& ins = instruments_[e.venue_instrument_id];
        if(!ins.empty()) {
            std::cout << "sym:'" << ins.venue_symbol() << "',"<<e<<std::endl;
        }
    }

    void on_instrument(const core::VenueInstrument& e) {
        //TOOLBOX_INFO  << e;
        instruments_.update(e);
    }

    void run(std::string url, const core::Parameters& params) {
        start_timestamp_ = tbs::MonoClock::now();
        gateway().url(url);
        gateway().parameters(params);
        gateway().state_changed().connect(tbu::bind<&This::on_state_changed>(this));
        gateway().ticks(ft::core::streams::BestPrice).connect(tbu::bind<&This::on_tick>(this));
        gateway().instruments(ft::core::streams::Instrument).connect(tbu::bind<&This::on_instrument>(this));
        gateway().start();
        gateway().stop();
    }

    int main(int argc, char* argv[]) {
        tbu::Options parser{"mdconv [OPTIONS] URL*"};
                
        //MdOptions opts;
        core::MutableParameters opts;
        std::vector<std::string> urls;

        bool help = false;
        
        tb::set_log_level(tb::Log::Debug);

        parser
            ('h', "help", tbu::Switch{help}, "print help")
            ('v', "verbose", tbu::Value{opts["verbose"], std::uint32_t{}}, "log level")
            ('I', "input_format", tbu::Value{opts["input_format"], std::string_view{}}.default_value("csv"), "input format")
            ('O', "output_format", tbu::Value{opts["output_format"], std::string_view{}}.default_value("csv"), "output format")
            ('d', "dst", tbu::Value{opts["filter"]["dst"], std::vector<std::string>{}}.multitoken(), "filter destination host(s)")
            ('p', "protocol", tbu::Value{opts["filter"]["protocols"], std::vector<std::string>{}}.multitoken(), "filter protocol(s), tcp or udp")
            ('m', "max_packet", tbu::Value{opts["filter"]["max_packet"], std::uint32_t{}}, "filter max packet")
            (tbu::Value{urls}.required().multitoken(), "URL");

        using SpbProtocol = spb::SpbUdpProtocol<tbn::PcapPacket>;
        using SpbMdGateway = pcap::PcapMdGateway<SpbProtocol>;
        
        using MdGwFactory = ftu::Factory<core::IMdGateway, core::MdGateway>;

        toolbox::json::MutableDocument json; 

        auto factory = MdGwFactory::unique_ptr(
            ftu::IdFn {"spb", [&] { return std::make_unique<SpbMdGateway>(); }}, 
            ftu::IdFn {"qsh", [&] { return std::make_unique<qsh::QshMdGateway>(); }});
        
        try {
            parser.parse(argc, argv);
            std::cout << "options:\n"<<opts<<"\n";
            gateway_ = factory(opts.value<std::string>("input_format"));
            for(auto& url: urls) {
                run(url, opts);
            }
        }catch(std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            help = true;
        }

        if(help) {
            std::cerr << std::endl << parser << std::endl;   
        }
        return EXIT_SUCCESS;
    }
private:
    tb::MonoTime start_timestamp_;
    std::unique_ptr<core::IMdGateway> gateway_;
    core::InstrumentsCache instruments_;
};

} // ns

int main(int argc, char* argv[]) {
    ft::MdConv app;
    return app.main(argc, argv);
}