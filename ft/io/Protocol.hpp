#pragma once

#include "ft/core/Component.hpp"
#include "toolbox/sys/Log.hpp"

namespace ft::io {


template<class Self>
class BasicProtocol: public core::BasicComponent<Self> {
    FT_MIXIN(Self)
    using Base = core::BasicComponent<Self>;
  public:
    /// open
    void open() {}
    /// close
    void close() {}
    /// client-initiated handshake    
    template<typename ConnT, typename DoneT>
    void async_handshake(ConnT& conn, DoneT done) {
        done({});
    }
    
    /// handle incoming packet
    template<typename ConnT, typename PacketT, typename DoneT>
    void async_handle(ConnT& conn, const PacketT& packet, DoneT done) { 
        TOOLBOX_ASSERT_NOT_IMPLEMENTED;
        done({});
    }
    /// write POD type
    template<typename ConnT, typename MessageT, typename DoneT>
    void async_write(ConnT& conn, const MessageT& m, DoneT done) {
        self()->async_write(conn, tb::to_const_buffer(m), std::forward<DoneT>(done));
    }
    /// write from buffer
    template<typename ConnT, typename DoneT>
    void async_write(ConnT& conn, tb::ConstBuffer buf, DoneT done) {
        conn.async_write(buf, std::forward<DoneT>(done));
    }
    /// read POD type
    template<typename ConnT, typename MessageT, typename DoneT>
    void async_read(ConnT& conn, MessageT& m, DoneT done) {
        self()->async_read(tb::to_mut_buffer(m), std::forward<DoneT>(done));
    }
    /// read into buffer
    template<typename ConnT, typename DoneT>
    void async_read(ConnT& conn, tb::MutableBuffer buf, DoneT done) {
        conn.async_read(buf, std::forward<DoneT>(done));
    }
}; // Protocol

} // ft::io