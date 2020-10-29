#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Executor.hpp"


#include <string_view>

namespace ft::core {


namespace streams {
    constexpr static const char* BestPrice = "BestPrice";
    constexpr static const char* Instrument = "Instrument";
};

using StreamType = const char*;

class IMdGateway : public IParameterized {
public:
    
    virtual ~IMdGateway() = default;
    /// 
    virtual void start() = 0;
    virtual void stop() = 0;
    
    // set url
    virtual void url(std::string_view url) = 0;
    virtual std::string_view url() const = 0;

    virtual RunState state() const = 0;    
    virtual RunStateSignal& state_changed() = 0;

    virtual TickStream& ticks(StreamType streamtype) = 0;

    virtual VenueInstrumentStream& instruments(StreamType streamtype) = 0;

    // very basic stats
    virtual core::StreamStats& stats() = 0;
};


template<typename ImplT>
class MdGateway : public IMdGateway {
public:
    MdGateway(std::unique_ptr<ImplT> &&impl)
    : impl_(std::move(impl)) {}

    void start() override { impl_->start(); }
    void stop() override { impl_->stop(); }
    
    void url(std::string_view url) { impl_->url(url);}
    std::string_view url() const { return impl_->url(); }

    RunState state() const override { return impl_->state(); }
    RunStateSignal& state_changed() override { return impl_->state_changed(); };

    core::StreamStats& stats() override { return impl_->stats(); }

    void parameters(const Parameters& parameters, bool replace=false) override { impl_->parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_->parameters(); }
    ParametersSignal& parameters_updated() override { return impl_->parameters_updated(); }
    
    core::TickStream& ticks(StreamType streamtype) override { return impl_->ticks(streamtype); }
    core::VenueInstrumentStream& instruments(StreamType streamtype) override { return impl_->instruments(streamtype); }
private:
    std::unique_ptr<ImplT> impl_;
};


template<typename DerivedT>
class BasicMdGateway : public BasicParameterized<Executor> {
    using Base = BasicParameterized<Executor>;
public:
    using State = RunState;
public:
    BasicMdGateway(tb::Reactor* reactor=nullptr)
    : Base(reactor) {
        Base::parameters_updated().connect(tbu::bind<&DerivedT::on_parameters_updated>(static_cast<DerivedT*>(this)));
    }
    BasicMdGateway(const BasicMdGateway& rhs) = delete;
    BasicMdGateway(BasicMdGateway&& rhs) = delete;

    void on_parameters_updated(const Parameters& params) {
        TOOLBOX_INFO << "BasicMdGateway::on_parameters_updated("<<"params="<<params<<")";
    }

    core::StreamStats& stats() { return impl().stats(); }
    
    void start() {
        state(State::Starting);
        state(State::Started);
        try {
            impl().run();
        }catch(const std::exception& e) {
            state(State::Failed, &e);
        }
    }

    void stop() {
        state(State::Stopping);
        state(State::Stopped);
    }

    //core::VenueInstrumentStream& instruments(StreamType streamtype)
    //TickStream& ticks(StreamType streamtype);
    //void run() {}
    
    //void url(std::string_view url) { impl().url(url); }
    //std::string_view url() const { return impl().url(); }

private:
    auto& impl() { return *static_cast<DerivedT*>(this); };
    const auto& impl() const { return *static_cast<const DerivedT*>(this); };
};



} // namespace ft::core