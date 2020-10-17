#pragma once
#include <cstdint>

namespace ft::core {

class StreamStats {
public:
    template<typename...ArgsT>
    void on_received(ArgsT...args) { total_received++; }
    
    template<typename...ArgsT>
    void on_accepted(ArgsT...args) { total_accepted++; }    

    template<typename...ArgsT>
    void on_bad(ArgsT...args) { total_bad++; }    

    std::size_t total_received{0};
    std::size_t total_accepted{0};
    std::size_t total_bad{0}; // rejected on bad format
};

} // namespace ft::core