#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "toolbox/util/Options.hpp"
#include "toolbox/sys/Log.hpp"
#include "TickMessage.hpp"
#include "QScalp.hpp"

using namespace toolbox;
using namespace ft;
using namespace ft::qscalp;

void handler(const TickMessage& e) {
    std::cout << e << std::endl;
}

int main(int argc, char* argv[]) {
    using namespace toolbox::util;
    Options opts{"mdconv [OPTIONS]"};
    int log_level = 0;
    std::string input_format;
    std::string output_format;
    std::vector<std::string> input_urls;
    std::string output_url;
    bool help = false;
    opts
        ('h',  "help",              Switch{help},                               "print help")
        ('v',   "verbose",          Value{log_level},                           "log level")
        ('i',   "input-format",    Value{input_format}
                                    .default_value("csv"),                      "input format")
        ('o',   "output-format",   Value{output_format}
                                    .default_value("csv"),                      "output format")
        ('s',   "output-url",      Value{output_url}.default_value("./out"),     "output url")
        ('l',   "input-url",       Value{input_urls}.multitoken().required(),    "input url");
    ;

    try {
        opts.parse(argc, argv);
        if(input_format == "qsh") {
            for(auto& input_url: input_urls) {
                auto start_ts = std::chrono::system_clock::now();
                std::size_t count = QScalp::parse(input_url, handler);
                auto elapsed = std::chrono::system_clock::now() - start_ts;
                std::cout << "parsed "<<std::dec<<count<< " records in " << elapsed.count()/1e9 <<" s" << " at "<< (1e3*count/elapsed.count()) << " mio/s"<<std::endl;
            }
            
        }
    }catch(std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        help = true;
    }
    if(help) {
        std::cerr << std::endl << opts << std::endl;   
    }
    return EXIT_SUCCESS;
}