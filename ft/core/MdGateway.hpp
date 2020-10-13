#pragma once

#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include <exception>
#include <string_view>
#include "toolbox/util/Json.hpp"
#include "toolbox/util/Slot.hpp"


namespace toolbox { namespace net { struct EndpointsFilter; }}

namespace ft::core {
using EndpointsFilter = toolbox::net::EndpointsFilter;

enum GatewayState:int {
    Stopped   = 0,
    Starting  = 1,
    Started   = 2,
    Stopping  = 3,
    Failed    = 4
};

using ExceptionPtr = const std::exception*;
using GatewayStateSignal = tbu::Signal<GatewayState, GatewayState, ExceptionPtr>;

class IMdGateway {
public:
    
    virtual ~IMdGateway() = default;
    /// 
    virtual void start() = 0;
    virtual void stop() = 0;
    /// print gateway diagnostics to stream
    virtual void report(std::ostream& os) const = 0;
    /// get gateway stats. TODO: return Json object to generalize and stabilize API
    virtual const core::StreamStats& stats() const = 0;
    
    virtual GatewayState state() const = 0;    
    virtual GatewayStateSignal& state_changed() = 0;

    /// configure from Json
    //virtual void configure(toolbox::json::Json) = 0;

    /// set connection url.
    virtual void url(std::string_view url) = 0;
    virtual std::string url() const = 0;
    virtual void filter(const EndpointsFilter& filter) = 0;
};

template<typename ImplT>
class MdGateway : public IMdGateway {
public:
    MdGateway(std::unique_ptr<ImplT> &&impl)
    : impl_(std::move(impl)) {}
    void report(std::ostream& os) const override { impl_->report(os); }
    const core::StreamStats& stats() const override { return impl_->stats(); }
    void url(std::string_view url) override { impl_->url(url); }
    std::string url() const override { return impl_->url(); }
    void start() override { impl_->start(); }
    void stop() override { impl_->stop(); }
    void filter(const EndpointsFilter& filter) override { impl_->filter(filter); }
    GatewayState state() const { return impl_->state(); }
    GatewayStateSignal& state_changed() override { return impl_->state_changed(); };
private:
    std::unique_ptr<ImplT> impl_;
};

template<typename DerivedT>
class BasicMdGateway {
public:
    core::StreamStats& stats() { return static_cast<DerivedT*>(this)->stats(); }
    /// set input (file path/url etc)
    void url(std::string_view url) { url_ = url; }
    std::string url() { return url_; }
    void start() {
        state(GatewayState::Starting);
        state(GatewayState::Started);
        try {
            static_cast<DerivedT*>(this)->run();
        }catch(const std::exception& e) {
            state_changed(GatewayState::Failed, &e);
        }
    }
    void stop() {
        state(GatewayState::Stopping);
        state(GatewayState::Stopped);
    }
    void run() {}
    void report(std::ostream& os) {}
    GatewayStateSignal& state_changed() { return state_changed_; }
    void state(GatewayState state, ExceptionPtr err=nullptr) {
        if(state != state_) {
            auto old_state = state_;
            state_ = state;
            state_changed_.invoke(state, old_state, err);
        }
    }
    GatewayState state() const { return state_; }
protected:
    std::string url_;
    GatewayState state_{GatewayState::Stopped};
    GatewayStateSignal state_changed_;
};



} // namespace ft::core