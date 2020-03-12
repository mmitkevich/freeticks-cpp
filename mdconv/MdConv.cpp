#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "toolbox/util/Options.hpp"
#include "toolbox/sys/Log.hpp"
#include "TickMessage.hpp"
#include "QScalp.hpp"

using namespace toolbox;

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
                QScalp::parse(input_url, handler);
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