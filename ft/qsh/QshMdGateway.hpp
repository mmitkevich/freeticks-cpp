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
    /// set input (file path/url etc)
    void input(std::string_view input) { input_ = input; }
    std::string_view input() { return input_; }
    /// read input file
    void run() {
        std::ifstream input_stream(input_);
        QshDecoder decoder(input_stream);
        decoder.decode();
        total_count_ = decoder.total_count();
    }
    std::size_t total_count() const { return total_count_; }
private:
    std::string input_;
    std::size_t total_count_{};
};

} // ns