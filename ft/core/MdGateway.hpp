#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Executor.hpp"
#include "ft/core/Component.hpp"


namespace ft::core {


namespace streams {
    constexpr static const char* BestPrice = "BestPrice";
    constexpr static const char* Instrument = "Instrument";
};

using StreamName = const char*;

class IMdGateway : public IComponent {
public:
    virtual TickStream& ticks(StreamName streamtype) = 0;

    virtual VenueInstrumentStream& instruments(StreamName streamtype) = 0;

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

    State state() const override { return impl_->state(); }
    StateSignal& state_changed() override { return impl_->state_changed(); };

    core::StreamStats& stats() override { return impl_->stats(); }

    void parameters(const Parameters& parameters, bool replace=false) override { impl_->parameters(parameters, replace); }
    const Parameters& parameters() const override { return impl_->parameters(); }
    ParametersSignal& parameters_updated() override { return impl_->parameters_updated(); }
    
    core::TickStream& ticks(StreamName stream) override { return impl_->ticks(stream); }
    core::VenueInstrumentStream& instruments(StreamName streamtype) override { return impl_->instruments(streamtype); }
private:
    std::unique_ptr<ImplT> impl_;
};


template<typename DerivedT, typename ExecutorT>
class BasicMdGateway : public BasicParameterized<ExecutorT>
{
    using This = BasicMdGateway<DerivedT, ExecutorT>;
    using Base = BasicParameterized<ExecutorT>;
public:
    using Executor = ExecutorT;
    using Reactor = typename Executor::Reactor;
    using State = typename Executor::State;
    using Executor::state;
public:
    BasicMdGateway(Reactor* reactor=nullptr)
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
        Base::start();
        try {
            impl().run();
        }catch(const std::exception& e) {
            state(State::Failed, &e);
        }
    }

    void stop() {
        Base::stop();
    }

    void url(std::string_view url) { url_=url;}
    std::string_view url() const { return url_; }


    //core::VenueInstrumentStream& instruments(StreamType streamtype)
    //TickStream& ticks(StreamType streamtype);
    //void run() {}
    
    //void url(std::string_view url) { impl().url(url); }
    //std::string_view url() const { return impl().url(); }

private:
    auto& impl() { return *static_cast<DerivedT*>(this); };
    const auto& impl() const { return *static_cast<const DerivedT*>(this); };
    std::string url_;
};



} // namespace ft::core