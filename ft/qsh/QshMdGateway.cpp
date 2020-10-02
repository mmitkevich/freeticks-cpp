#include "QshMdGateway.hpp"

using namespace ft::qsh;

void QshMdGateway::input(std::string_view input) {
    input_ = input;
}

std::string_view QshMdGateway::input() {
    return input_;
}

void QshMdGateway::run() {
    std::ifstream input_stream(input_);
    decoder_.decode(input_stream);
}