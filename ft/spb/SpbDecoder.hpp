#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/pcap/PcapMdGateway.hpp"
#include "ft/utils/Common.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/Packet.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <cstdint>
#include <iomanip>
#include <unordered_map>
#include "ft/spb/SpbFrame.hpp"
#include "toolbox/net/Protocol.hpp"
#include "toolbox/util/Tuple.hpp"
#include "ft/core/SequenceMultiplexor.hpp"
namespace ft::spb {

class Frame;

class SpbDecoderStats: public core::BasicStats<SpbDecoderStats> {
public:
    using Base = core::BasicStats<SpbDecoderStats>;
    using MsgId = uint32_t;
    using MsgStats = utils::FlatMap<MsgId, std::size_t>;
public:
    void on_report(std::ostream& os) {
        std::size_t n = values_.size();
        os <<"msg_stat, distinct:"<<n<<std::endl;
        for(auto& [k,v]: values_) {
            os << std::setw(12) << v << "    " << k << std::endl;
        }
    }
    void on_received(const Frame& frame) {
        Base::on_received(frame);
        auto current = values_[frame.msgid];
        values_[frame.msgid] = current+1;
    }
protected:
    MsgStats values_;
};

template<typename DerivedT, typename ProtocolT, typename BaseT> 
class BasicSpbStream : public BaseT {
    using Base = BaseT;
public:
    using Protocol = ProtocolT;
    using Decoder = typename Protocol::Decoder;
    template<typename ImplT>
    using SequenceMultiplexor = typename Decoder::template SequenceMultiplexor<ImplT>;
    using Schema = typename Decoder::Schema;
    template<typename MessageT>
    using SpbPacket = typename Decoder::template TypedPacket<MessageT>;
    using Multiplexor = SequenceMultiplexor<DerivedT>;
public:
    BasicSpbStream(Protocol& protocol)
    : protocol_(protocol) {
        auto& ex = protocol.executor();
        ex.state_changed().connect(tb::bind<&DerivedT::on_gateway_state_changed>(&impl()));
    }
    
    BasicSpbStream(const BasicSpbStream&) = delete;
    BasicSpbStream(BasicSpbStream&&) = delete;

    Protocol& protocol() { return protocol_; }
    Decoder& decoder() { return protocol_.decoder_; }
    
    core::Executor& executor() { return protocol_.executor(); }

    template<typename PacketT>
    void on_decoded(const PacketT& packet) {
        if(update_.match(packet))
            update_.on_packet(packet);
        else if(snapshot_.match(packet))
            snapshot_.on_packet(packet);
    }
    template<typename PacketT>
    void on_stale(const PacketT& packet, const Multiplexor& mux) {
        //TOOLBOX_DEBUG << DerivedT::name()<<"."<<mux.name()<<": ignored stale seq "<<packet.sequence()<<" current seq "<<mux.sequence();
    }
    void on_gateway_state_changed(core::RunState state, core::RunState old_state, core::ExceptionPtr err) {}
protected:
    void on_parameters_updated(const core::Parameters &params) {
        std::string_view type = params["type"].get_string();
        if(type == snapshot_.name()) {
            snapshot_.on_parameters_updated(params);
        } else if(type == update_.name()) {
            update_.on_parameters_updated(params);
        }
    }
protected:
    DerivedT& impl() { return *static_cast<DerivedT*>(this); }
protected:
    Protocol& protocol_;
    Multiplexor snapshot_{impl(), "snapshot.mcast"};
    Multiplexor update_{impl(), "update.mcast"};
};


template<typename MessageT> constexpr bool spb_msgid_v = MessageT::msgid;

template<typename FrameT, typename SchemaT, typename BinaryPacketT, typename StreamsTuple>
class SpbDecoder
{
public:
    using BinaryPacket = BinaryPacketT;
    using Schema = SchemaT;
    
    template<typename ImplT>
    using SequenceMultiplexor = core::BasicSequencedMultiplexor<ImplT, typename BinaryPacket::Header::Endpoint, std::uint64_t>;

    template<typename MessageT> 
    class TypedPacket: public tb::PacketView<MessageT, BinaryPacket> {
        using Base = tb::PacketView<MessageT, BinaryPacket>;
    public:
        using Base::Base;
        using Base::value;
        using Base::data;
        using Base::size;
        using Base::header;
        std::uint64_t sequence() const { return value().header().frame.seq; }
    };
public:
    SpbDecoder(const SpbDecoder&) = delete;
    SpbDecoder(SpbDecoder&&) = delete;
    SpbDecoder(StreamsTuple&& streams)
        : streams_(streams) { }
    
    static constexpr std::string_view name() { return "SPB_MDBIN"; }

    void on_packet(const BinaryPacket& packet) {
        const char* begin = packet.data();
        const char* ptr = begin;
        const char* end = begin + packet.size();
        while(ptr < end) {
            const Frame &frame = *reinterpret_cast<const Frame*>(ptr);
            //TOOLBOX_DEBUG << name()<<": recv "<<packet.size()<<" bytes from "<<packet.src()<<" seq "<<frame.seq;
            FT_ASSERT(frame.size>=0);
            
            if(frame.size<=0) {
                TOOLBOX_DEBUG << "["<<(ptr-begin)<<"] invalid size "<<frame.size;
                stats_.on_bad(frame);
                break;
            }

            stats_.on_received(frame);

            bool found = false;
            tb::tuple_for_each(streams_, [&](auto &stream) {
                using Stream = std::decay_t<decltype(stream)>;
                using TypeList = typename Stream::TypeList;//Message = typename CB::value_type::value_type;

                mp::mp_for_each<TypeList>([&](auto &&message) {
                    using Message = std::decay_t<decltype(message)>;
                    if(!found && Message::msgid == frame.msgid) {
                        found = true;
                        //TOOLBOX_DEBUG << "["<<(ptr-begin)<<"] msgid "<<frame.msgid<<" seq "<<frame.seq;
                        TypedPacket<Message> typed_packet(packet);
                        stats_.on_accepted(frame);
                        stream.on_decoded(typed_packet);
                    }
                });
            });
            //if(!found) {
            //    TOOLBOX_DEBUG << name()<<": ["<<(ptr-begin)<<"] unknown msgid "<<frame.msgid;
            //}
            ptr += sizeof(Frame) + frame.size;
        }
    }
    SpbDecoderStats& stats() { return stats_; }
private:
    StreamsTuple streams_;
    SpbDecoderStats stats_;
};

}