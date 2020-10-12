#pragma once
#include <cstdint>

namespace ft::core {

class StreamStats {
public:
    std::size_t total_received{0};
    std::size_t total_accepted{0};
};

} // namespace ft::core