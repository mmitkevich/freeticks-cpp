#pragma once

#include "ft/utils/Common.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/core/StreamStats.hpp"

namespace ft::core {

template<typename MessageT>
class Stream : public tbu::Signal<const MessageT&>
{
    using Base = tbu::Signal<const MessageT&>;
public:
    using Base::Base;
    using Base::connect;
    using Base::disconnect;

    core::StreamStats& stats() { return stats_; }
protected:
    core::StreamStats stats_;
};


template<typename SequenceId, typename BaseT>
class SequencedStream : public BaseT {
public:
    SequenceId sequence() { return sequence_; }
    void sequence(SequenceId val) { sequence_ = val; }
protected:
    SequenceId sequence_{};
};

};