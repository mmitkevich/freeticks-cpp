#pragma once

#include "ft/utils/Common.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/StreamStats.hpp"

namespace ft::core {


template<typename SequenceId>
class Sequenced {
public:
    SequenceId sequence() const { return sequence_; }
    void sequence(SequenceId val) { sequence_ = val; }
protected:
    SequenceId sequence_{};
};


template<typename MessageT>
class Stream : public tb::Signal<const MessageT&>, public Sequenced<std::uint64_t>
{
    using Base = tb::Signal<const MessageT&>;
    using Sequenced = Sequenced<std::uint64_t>;
public:
    using Base::Base;
    using Base::connect;
    using Base::disconnect;
    using Sequenced::sequence;

    core::StreamStats& stats() { return stats_; }
protected:
  core::StreamStats stats_;
};

};