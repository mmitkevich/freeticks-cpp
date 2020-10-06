#pragma once

#include "ft/utils/Common.hpp"
#include "toolbox/util/Slot.hpp"

namespace ft::core {

template<typename MessageT>
class Stream : public tbu::Signal<const MessageT&> {
    using Base=tbu::Signal<const MessageT&>;
public:
    using Base::Base;
    using Base::connect;
    using Base::disconnect;
};

};