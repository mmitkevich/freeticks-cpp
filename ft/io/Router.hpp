#pragma once
#include <utility>
#include "ft/utils/Common.hpp"
#include <toolbox/util/Slot.hpp>
#include <toolbox/io/Socket.hpp>

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

/// Router = AsyncWrite + ConnectionsAware
template<typename DerivedT>
class BasicRouter {
    // crtp boilerplate
    using This = BasicRouter<DerivedT>;
    using Self = DerivedT; 

    Self* self() { return static_cast<Self*>(this); }
    const Self* self() const { return static_cast<const Self*>(this); }
public:
    // ConnectionsMap connections()  = 0;

    template<typename MessageT, typename RouteT=RouteAll>
    void async_write(const MessageT& m, tb::SizeSlot done, RouteT route=RouteT{}) {
        if constexpr(std::is_same_v<RouteNone, RouteT>)
            return;
        assert(write_.empty());            
        int pending = 0;
        for(auto& [ep, conn] : self()->connections()) {
            if(route(conn, m)) {
               pending++;                
               self()->async_write(conn, m, tb::bind([this](ssize_t size, std::error_code ec) {
                   write_.notify(size, ec);
               }));
            }
        }
        write_.pending()=pending;
    } 

    template<typename ConnT, typename MessageT>
    void async_write(ConnT& conn, const MessageT& m, tb::SizeSlot slot) {
        conn.async_write(tb::ConstBuffer{&m, m.length()}, slot);
    }
protected:
    tb::PendingSlot<ssize_t, std::error_code> write_;
};

};