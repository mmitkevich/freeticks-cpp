#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"


namespace ft::core {


class IMdServer : public IComponent {
public:
    virtual SubscriptionSignal& subscriptions(StreamName id) = 0;

    // "instruments"
    virtual core::InstrumentSink& instruments(StreamName id) = 0;
    // "bestprice", "orderbook", etc
    virtual core::TickSink& ticks(StreamName id) = 0;
};


template<typename ImplT>
class MdServerImpl : public IMdServer {
public:
    MdServerImpl(std::unique_ptr<ImplT> &&impl)
    : impl_(std::move(impl)) {}

    void start() override { impl_->start(); }
    void stop() override { impl_->stop(); }
    
    void url(std::string_view url) { impl_->url(url);}
    std::string_view url() const { return impl_->url(); }

    State state() const override { return impl_->state(); }
    StateSignal& state_changed() override { return impl_->state_changed(); };

    void parameters(const Parameters& parameters, bool replace=false) override { impl_->parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_->parameters(); }
    
    SubscriptionSignal& subscriptions(StreamName id) override { return impl_->subscriptions(id); }
    core::TickSink& ticks(StreamName stream) override { return impl_->ticks(stream); }
    core::InstrumentSink& instruments(StreamName streamtype) override { return impl_->instruments(streamtype); }
private:
    std::unique_ptr<ImplT> impl_;
};



} // namespace ft::core