#pragma once

#include "ft/core/Component.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/Buffer.hpp"
#include "toolbox/sys/Log.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Requests.hpp"

namespace ft::io {

template<class Self, typename...>
class BasicSubscribable {
    FT_SELF(Self);
public:
    core::SubscriptionSignal& subscription() { 
      return subscription_;
    }
    /// implements subscription of peer
    template<class PeerT>
    void on_subscribe(PeerT& conn, core::SubscriptionRequest& req) {
        TOOLBOX_INFO << "on_subscribe: " << req;
        self()->subscription().invoke(conn.id(), std::move(req));
    }
  protected:
    core::SubscriptionSignal subscription_;
};

template<class Self, typename...>
class BasicSignalSlot {
    FT_SELF(Self);
public:
    core::Stream& signal(StreamTopic topic) {
        std::stringstream ss;
        ss << "no signal for topic "<<topic;
        throw std::logic_error(ss.str());
    }

    core::Stream& slot(StreamTopic topic) {
        std::stringstream ss;
        ss << "no slot for topic "<<topic;
        throw std::logic_error(ss.str());
    }

    template<typename T>
    class Slot : public core::Stream::BasicSlot<Slot<T>, const T&, tb::SizeSlot> {
      public:
        Slot(Self* parent)
        : parent_(parent) {}

        void invoke(const T&e, tb::SizeSlot done) {
            parent_->async_write(e, done);
        }
      protected:
        Self* parent_;
    };

    template<typename T>
    using Signal = core::Stream::Signal<const T&, tb::SizeSlot>;
};

template<class Self, typename...>
class BasicPeerWriter {
    FT_SELF(Self)
public:
    /// write to peer as POD
    template<class PeerT, typename MessageT, typename DoneT>
    void async_write_to(PeerT& peer, const MessageT& m, DoneT done) {
        self()->async_write_to(peer, tb::to_const_buffer(m), std::forward<DoneT>(done));
    }
    /// write to peer
    template<class PeerT, typename MessageT, typename DoneT>
    void async_write_to(PeerT& peer, tb::ConstBuffer buf, DoneT done) {
        peer.async_write(buf, std::forward<DoneT>(done));
    }
};

template<class Self, typename...>
class BasicPeerReader {
    FT_SELF(Self)
public:
    /// read from peer as POD
    template<class PeerT, typename MessageT, typename DoneT>
    void async_read_from(PeerT& peer, MessageT& m, DoneT done) {
        self()->async_read(tb::to_mut_buffer(m), std::forward<DoneT>(done));
    }
    /// read from peer
    template<class PeerT, typename DoneT>
    void async_read_from(PeerT& peer, tb::MutableBuffer buf, DoneT done) {
        peer.async_read(buf, std::forward<DoneT>(done));
    }
};

template<class Self, typename...>
class BasicProtocol
: public BasicSignalSlot<Self>
, public BasicSubscribable<Self>
, public BasicPeerWriter<Self> {
    FT_SELF(Self);
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

    void on_parameters_updated(const core::Parameters& params) {

    }
}; // Protocol

template<class Self, typename...O>
class BasicMdProtocol : public BasicProtocol<Self, O...> {
    using Base = BasicProtocol<Self, O...>;
    FT_SELF(Self);
  public:
    core::Stream& signal(core::StreamTopic topic) {
        switch(topic) {
            case core::StreamTopic::BestPrice: return self()->bestprice();
            case core::StreamTopic::Instrument: return self()->instruments();
            default: return Base::signal(topic);
        } 
    }
};
} // ft::io