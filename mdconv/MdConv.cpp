#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <boost/mp11.hpp>

#include "toolbox/util/Finally.hpp"
#include "toolbox/util/Options.hpp"
#include "toolbox/sys/Log.hpp"

#include "ft/pcap/PcapMdGateway.hpp"
#include "ft/qsh/QshMdGateway.hpp"

#include "ft/spb/SpbProtocol.hpp"

#include "Task.hpp"


namespace ft {

namespace tbu = toolbox::util;
namespace tb = toolbox;

//void handler(const ft::api::TickMessage& e) {
//    std::cout << e << std::endl;
//}

class MdConv {
public:
    struct MdFilter {
        std::optional<std::string> dst_host;
        std::optional<std::uint32_t> dst_port;
    };
    struct MdOptions {
        int log_level = 0;
        std::string input_format;
        std::string output_format;
        std::vector<std::string> inputs;
        std::optional<std::string> output;
        MdFilter filter;
    };
public:
    int main(int argc, char* argv[]) {
        tbu::Options parser{"mdconv [OPTIONS]"};
                
        MdOptions opts;
        bool help = false;
        
        tb::set_log_level(tb::Log::Debug);

        parser
            ('h',  "help",          tbu::Switch{help},                                                        "print help")
            ('v',   "verbose",      tbu::Value{opts.log_level},                                               "log level")
            ('I',   "input_format", tbu::Value{opts.input_format}.default_value(      "csv"),                 "input format")
            ('O',   "output_format",tbu::Value{opts.output_format}.default_value(     "csv"),                 "output format")
            ('d',   "dst_host",     tbu::Value{opts.filter.dst_host},                                         "destination host")
            ('p',   "dst_port",     tbu::Value{opts.filter.dst_port},                                         "destination port")
            ('o',   "output",       tbu::Value{opts.output}.default_value("./out"),                           "output path")
            ('i',   "inputs",       tbu::Value{opts.inputs}.multitoken().required(),                          "input paths");

        auto spb_proto = spb::SpbUdpProtocol {};
        auto gw = pcap::PcapMdGateway {std::move(spb_proto)};

        auto tasks = make_task_set(
            Task("spb", std::move(gw))
        );

        try {
            parser.parse(argc, argv);
            tasks.run(opts.input_format, opts);
        } catch(std::runtime_error &e) {
            std::cerr << e.what() << std::endl;
            help = true;
        }
        if(help) {
            std::cerr << std::endl << parser << std::endl;   
        }
        return EXIT_SUCCESS;
    }
};

} // ns

int main(int argc, char* argv[]) {
    ft::MdConv app;
    return app.main(argc, argv);
}