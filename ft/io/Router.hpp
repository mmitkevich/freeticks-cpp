#pragma once
#include <utility>
#include "ft/utils/Common.hpp"
#include <toolbox/util/Slot.hpp>

namespace ft::io {


class RouteAll {
  public:
    template<typename ConnT, typename MessageT>
    bool operator()(ConnT, MessageT) const { return true; }
};

class RouteNone {
  public:
    template<typename ConnT, typename MessageT>
    bool operator()(ConnT, MessageT) const { return false; }
};

template<typename DerivedT>
class BasicRouter {
    // crtp boilerplate
    using This = BasicRouter<DerivedT>;
    using Self = DerivedT; 
    Self* self() { return static_cast<Self*>(this); }
    const Self* self() const { return static_cast<const Self*>(this); }

public:
    template<typename MessageT, typename RouteT=RouteAll>
    void async_write(const MessageT& m, tb::DoneSlot slot, const RouteT& route={}) {
        if constexpr(std::is_same_v<RouteNone, RouteT>)
            return;
        assert(!write_);
        write_ = slot;
        pending_write_ = 0;
        for(auto& [ep, conn] : self()->connections()) {
            if(route(conn, m)) {
               pending_write_++;                
               self()->async_write(conn, m, toolbox::bind<&This::on_write_>(self()));
            }
        }
    } 

    template<typename ConnT, typename MessageT>
    void async_write(ConnT& conn, const MessageT& m, tb::DoneSlot slot) {
        conn.async_write(m, slot);
    }

    template<typename ConnT, typename MessageT>
    void async_read(ConnT& conn, MessageT& m, tb::DoneSlot slot) {
        conn.async_read(m, slot);
    }
protected:
    /// CRTP overrides
    void on_write(ssize_t size, std::error_code ec) {}
private:
    void on_write_(ssize_t size, std::error_code ec) {
        if(--pending_write_==0) {
            self()->on_write(size, ec);
        }
    }
protected:
    tb::DoneSlot write_;
    int pending_write_{};    
};

};