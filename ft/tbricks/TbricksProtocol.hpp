#pragma once
#include "boost/mp11/bind.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/matching/OrderBook.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Requests.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/core/Instrument.hpp"
#include "ft/tbricks/TbricksSchemaV1.hpp"
#include "ft/core/Parameters.hpp"
#include "toolbox/io/Buffer.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/util/Slot.hpp"
#include "ft/io/Service.hpp"
#include "ft/io/Conn.hpp"
#include <stdexcept>
#include <system_error>
#include "ft/utils/StringUtils.hpp"
#include "ft/core/BestPriceCache.hpp"
namespace ft::tbricks {

// plugin mixin
template<class Self, typename...>
class TbricksProtocol : public io::BasicMdProtocol<Self> {
    FT_SELF(Self);
protected:    
    using Base = io::BasicMdProtocol<Self>;
public:
    using Message = tbricks::v1::Message<0>;
    using MessageOut = tbricks::v1::Message<4096>;
    using MessageType = tbricks::v1::MessageType;
    using Sequence = tbricks::v1::Sequence;

    using Base::Base;
    
    using BestPriceSignal = typename Base::template Signal<core::Tick>;
    using InstrumentSignal = typename Base::template Signal<core::InstrumentUpdate>;

    using Base::async_write_to;
    
    template<typename ConnT, typename DoneT>
    void async_write_to(ConnT& conn, const SubscriptionRequest& request, DoneT done) {
        // remember request id
        switch(request.request()) {
            case core::Request::Subscribe: {
                // FIXME: redo as stream << into buffer?
                MessageOut msg(MessageType::SubscriptionRequest);
                msg.symbol() = request.symbol();
                msg.seq() = ++out_seq_;
                auto buf = tb::to_const_buffer(msg);
                TOOLBOX_INFO << name()<<": out["<<msg.bytesize()<<"]: seq="<<msg.seq()<<"; sym="<<msg.symbol().str()<<"\n" << ft::to_hex_dump(std::string_view{(const char*)buf.data(), buf.size()});
                conn.async_write(buf, done);
            } break;
            default: {
                assert(false);
                done(0, {}); // should be always called
            }
        }
    }

    template<typename ConnT, typename DoneT>
    void async_write_to(ConnT& conn, const core::Tick& ticks, DoneT done) {
        core::BestPrice& bp = bestprice_cache_.update(ticks);

        MessageOut msg(MessageType::MarketData);
        assert(this->instruments_cache());
        msg.symbol() = this->instruments_cache()->symbol(ticks.venue_instrument_id());
        msg.marketdata().time() = tbricks::v1::Timestamp { ticks.send_time() };
        for(auto& e: ticks) {
            switch(e.side()) {
                case core::TickSide::Buy: {
                    double price = bp.price_conv().to_double(bp.bid_price());
                    bool empty = !bp.test(core::Field::BidPrice);
                    msg.marketdata().bid().value = empty ? NAN:price;
                    msg.marketdata().bid().empty = empty;
                } break;
                case core::TickSide::Sell:
                    double price = bp.price_conv().to_double(bp.ask_price());
                    bool empty = !bp.test(core::Field::AskPrice);
                    msg.marketdata().ask().value = empty ? NAN: price;
                    msg.marketdata().ask().empty = empty;
                    break;
            }
        }
        msg.seq() = ++out_seq_;
        auto buf = tb::to_const_buffer(msg);
        TOOLBOX_INFO << name()<<": out["<<msg.bytesize()<<"]: seq="<<msg.seq()<<"; sym="<<msg.symbol().str()<<"\n" << ft::to_hex_dump(std::string_view{(const char*)buf.data(), buf.size()});
        conn.async_write(buf, done);
    }

    constexpr std::string_view name() { return "TB1"; }
    
    template<class ConnT, class PacketT, class DoneT>
    void async_handle(ConnT& conn, const PacketT& e, DoneT done) { 
        std::error_code ec{};

        auto& buf = e.buffer();
        const Message& msg = *reinterpret_cast<const Message*>(buf.data());        
        TOOLBOX_DEBUG << name()<<": in["<<e.buffer().size()<<"]: t="<<(int64_t)tb::unbox(msg.msgtype())<<"\n"<<
            ft::to_hex_dump(std::string_view{(const char*)buf.data(), buf.size()});
        switch(msg.msgtype()) {
            case MessageType::SubscriptionRequest: {
                TOOLBOX_DEBUG << name()<<": in: t:"<<(int)tb::unbox(msg.msgtype())<<", sym:'"<<msg.symbol().str()<<"', seq:"<<msg.seq();
                //if constexpr(TB_IS_VALID(self(), self()->subscription())) {
                    core::SubscriptionRequest req {};
                    req.symbol(msg.symbol().str());
                    req.request(Request::Subscribe);
                    req.topic(StreamTopic::BestPrice);
                    std::size_t id = std::hash<std::string_view>{}(msg.symbol().str());
                    req.instrument_id(InstrumentId(id));
                    self()->on_subscribe(conn, req);
                //}
            } break;
            case MessageType::SubscriptionCancelRequest: {
                if constexpr(TB_IS_VALID(self(), self()->subscription())) {
                    core::SubscriptionRequest req;
                    req.symbol(msg.symbol().str());
                    req.request(core::Request::Unsubscribe);
                    self()->on_subscribe(conn, req);
                }
            } break;
            case MessageType::ClosingEvent: {
                if constexpr(TB_IS_VALID(self(), self()->subscription())) {
                    core::SubscriptionRequest req;
                    req.symbol(msg.symbol().str());
                    req.request(core::Request::Close);
                    self()->on_subscribe(conn, req);
                }
            } break;
            case MessageType::HeartBeat: {
                if constexpr(io::HeartbeatsTraits::has_heartbeats<ConnT>) {
                    conn.on_heartbeats();
                }
            } break;
            case MessageType::MarketData: {
                //core::Ticks<2> ticks;
                //bestprice().invoke();
                TOOLBOX_DEBUG<<"MarketData bid:"<<(msg.marketdata().bid().empty ? NAN: msg.marketdata().bid().value)
                    << " ask:"<<(msg.marketdata().ask().empty ? NAN : msg.marketdata().ask().value)
                    << " symbol:"<<msg.symbol().str();
                core::Ticks<2> ticks;
                std::size_t i = 0;
                ticks.topic(StreamTopic::BestPrice);
                std::size_t id = std::hash<std::string_view>{}(msg.symbol().str());
                ticks.instrument_id(InstrumentId{id});
                ticks.venue_instrument_id(VenueInstrumentId{id});
                ticks.send_time(msg.marketdata().time().to_core_timestamp());
                ticks.event(core::Event::Update);
                if(!msg.marketdata().bid().empty) {
                    ticks[i] = {};
                    ticks[i].side(TickSide::Buy);
                    ticks[i].event(TickEvent::Modify);
                    ticks[i].price(TickElement::price_conv().from_double(msg.marketdata().bid().value));
                    i++;
                }
                if(!msg.marketdata().ask().empty) {
                    ticks[i] = {};
                    ticks[i].side(TickSide::Sell);
                    ticks[i].event(TickEvent::Modify);
                    ticks[i].price(TickElement::price_conv().from_double(msg.marketdata().ask().value));
                    i++;
                }
                ticks.resize(i);
                bestprice().invoke(ticks.as_size<1>(), nullptr);
            } break;
            case MessageType::MarketDataBBO: {
                
            } break;
            default:
                TOOLBOX_ERROR << "TbricksV1: unknown msgtype "<<tb::unbox(msg.msgtype());
                ec = std::make_error_code(std::errc::invalid_argument);
        }
        done(ec);
    }

    auto& stats() { return stats_; }
    
    auto& bestprice() { return bestprice_signal_; }
    auto& instruments() { return instruments_signal_; }
protected:
    core::BestPriceCache bestprice_cache_;
    // from server to client
    BestPriceSignal bestprice_signal_;
    InstrumentSignal instruments_signal_;

    core::StreamStats stats_;
    // from client to server 
    Sequence out_seq_ {};
    RequestId  out_req_id_;
    // from server
    Message response_;
};

}