#pragma once

#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include <string_view>

namespace toolbox { namespace net { struct EndpointsFilter; }}

namespace ft::core {
using EndpointsFilter = toolbox::net::EndpointsFilter;

class IMdGateway {
public:
    enum State:int {
        Stopped,
        Starting,
        Started,
        Stopping,
        Failed
    };
    virtual ~IMdGateway() = default;
    /// 
    virtual void start() = 0;
    virtual void stop() = 0;
    /// print gateway diagnostics to stream
    virtual void report(std::ostream& os) = 0;
    /// get gateway stats. TODO: return Json object to generalize and stabilize API
    virtual core::StreamStats& stats() = 0;
    
    /// configure from Json
    //virtual void configure(std::string config) = 0;
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
    void report(std::ostream& os) override { impl_->report(os); }
    core::StreamStats& stats() override { return impl_->stats(); }
    void url(std::string_view url) override { impl_->url(url); }
    std::string url() const override { return impl_->url(); }
    void start() override { impl_->start(); }
    void stop() override { impl_->stop(); }
    void filter(const EndpointsFilter& filter) override { impl_->filter(filter); }
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
    void start() {}
    void stop() {}
    void report(std::ostream& os) {}
protected:
    std::string url_;
};



} // namespace ft::core