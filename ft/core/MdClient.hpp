#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"


namespace ft::core {


namespace streams {
    constexpr static const char* BestPrice = "BestPrice";
    constexpr static const char* Instrument = "Instrument";
};


class IMdClient : public IComponent {
public:
    virtual TickStream& ticks(StreamName streamtype) = 0;

    virtual InstrumentStream& instruments(StreamName streamtype) = 0;

    // very basic stats
    virtual core::StreamStats& stats() = 0;
};


// wrap MD client into dynamic interface
template<typename ImplT>
class MdClientImpl : public IMdClient {
public:
    template<typename...ArgsT>
    MdClientImpl(ArgsT...args)
    : impl_(std::forward<ArgsT>(args)...) {}

    void start() override { impl_.start(); }
    void stop() override { impl_.stop(); }
    
    void url(std::string_view url) { impl_.url(url);}
    std::string_view url() const { return impl_.url(); }

    State state() const override { return impl_.state(); }
    StateSignal& state_changed() override { return impl_.state_changed(); };

    core::StreamStats& stats() override { return impl_.stats(); }

    void parameters(const Parameters& parameters, bool replace=false) override { impl_.parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_.parameters(); }
    
    core::TickStream& ticks(StreamName stream) override { return impl_.ticks(stream); }
    core::InstrumentStream& instruments(StreamName streamtype) override { return impl_.instruments(streamtype); }
private:
    ImplT impl_;
};



} // namespace ft::core