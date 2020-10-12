#pragma once

#include "ft/core/Stream.hpp"

namespace ft::core {

class MdGateway {
public:
    virtual void run() {}
    virtual void report(std::ostream& os) {}
    core::StreamStats& stats() { return stats_; }
    /// set input (file path/url etc)
    void input(std::string_view input) { input_ = input; }
    std::string_view input() { return input_; }
protected:
    core::StreamStats stats_;
    std::string input_;
};
} // namespace ft::core