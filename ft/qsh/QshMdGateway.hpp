#pragma once
#include "ft/utils/Common.hpp"
#include "QshDecoder.hpp"

namespace ft::qsh {

// file-based gateway for Qsh files
class QshMdGateway {
public:
    /// setup filter
    template<typename FilterT>
    void filter(const FilterT &flt) {}

    /// run gateway
    void run();
    
    /// set input (file path/url etc)
    void input(std::string_view file);
    /// get input
    std::string_view input();
private:
    QshDecoder decoder_;
    std::string input_;
};

} // ns