#pragma once

#include "ft/utils/Common.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/StreamStats.hpp"

namespace ft::core {

template<typename MessageT>
class Stream : public tbu::Signal<MessageT>
{
    using Base = tbu::Signal<MessageT>;
public:
    using Base::Base;
    using Base::connect;
    using Base::disconnect;

    core::StreamStats& stats() { return stats_; }
protected:
    core::StreamStats stats_;
};

};