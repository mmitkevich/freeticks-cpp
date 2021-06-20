#pragma once

#include "ft/core/Instrument.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/StreamStats.hpp"
#include "ft/core/Tick.hpp"
//#include "ft/io/PcapMdClient.hpp"
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
#include "ft/core/SequencedChannel.hpp"
namespace ft::spb {

class Frame;

class SpbDecoderStats: public core::BasicStats<SpbDecoderStats> {
public:
    using Base = core::BasicStats<SpbDecoderStats>;
    using MsgId = uint32_t;
    using MsgStats = ft::unordered_map<MsgId, std::size_t>;
public:
    void on_report(std::ostream& os) {
        if constexpr(core::ft_stats_enabled()) {
            std::size_t n = values_.size();
            os <<"msg_stat["<<n<<"]:"<<std::endl;
            for(auto [k,v]: values_) {
                os << std::setw(12) << v << "    " << k << std::endl;
            }
        }
    }
    void on_received(const Frame& frame) {
        Base::on_received(frame);
        if constexpr(core::ft_stats_enabled()) {
            auto current = values_[frame.msgid];
            values_[frame.msgid] = current+1;
        }
    }
protected:
    MsgStats values_;
};

template<class Self, class T, template<class> class ChannelTT, typename...> 
class BasicSpbStream : public Stream::Signal<const T&> {
    FT_SELF(Self);
    using Base = Stream::Signal<const T&>;
public:
    using Base::Base;
    using Channel = ChannelTT<Self>;
    //BasicSpbStream(const BasicSpbStream&) = delete;
    //BasicSpbStream(BasicSpbStream&&) = delete;

    void open() {}
    
    template<typename PacketT>
    void on_decoded(const PacketT& packet) {
        if(update_.match(packet))
            update_.on_packet(packet);
        else if(snapshot_.match(packet))
            snapshot_.on_packet(packet);
    }
    template<typename PacketT>
    void on_stale(const PacketT& packet) {
        //TOOLBOX_DEBUG << DerivedT::name()<<"."<<mux.name()<<": ignored stale seq "<<packet.sequence()<<" current seq "<<mux.sequence();
    }
protected:
    void on_parameters_updated(const core::Parameters& params) {
        std::string_view type = params["type"].get_string();
        if(type == snapshot_.name()) {
            snapshot_.on_parameters_updated(params);
        } else if(type == update_.name()) {
            update_.on_parameters_updated(params);
        }
    }
protected:
    Channel snapshot_{*self(), "snapshot"};
    Channel update_{*self(), "update"};
};


template<typename MessageT> constexpr bool spb_msgid_v = MessageT::msgid;

template<class FrameT, class SchemaT, typename StreamsTuple>
class SpbDecoder
{
public:
    using Schema = SchemaT;
    using Frame = FrameT;
    template<class MessageT, class BinaryPacketT> 
    using Packet = typename Schema::template Packet<MessageT, BinaryPacketT>;
public:
    SpbDecoder(const SpbDecoder&) = delete;
    SpbDecoder(SpbDecoder&&) = delete;
    template<typename...StreamsT>
    SpbDecoder(StreamsTuple&& streams)
        : streams_(std::move(streams)) { }


    template<class BinaryPacketT>
    void operator()(const BinaryPacketT& packet) {
        auto& buf = packet.buffer();
        const char* begin = reinterpret_cast<const char*>(buf.data());
        const char* ptr = begin;
        const char* end = begin + buf.size();
        while(ptr < end) {
            const Frame &frame = *reinterpret_cast<const Frame*>(ptr);
            //TOOLBOX_DEBUG << name()<<": recv "<<packet.size()<<" bytes from "<<packet.src()<<" seq "<<frame.seq;
            FT_ASSERT(frame.size>=0);
            
            if(frame.size<=0) {
                TOOLBOX_DEBUG << "["<<(ptr-begin)<<"] invalid size "<<frame.size;
                stats_.on_rejected(frame);
                break;
            }

            stats_.on_received(frame);

            bool found = false;
            mp::tuple_for_each(streams_, [&](auto* stream) {
                using Stream = std::decay_t<decltype(*stream)>;
                using TypeList = typename Stream::TypeList;//Message = typename CB::value_type::value_type;

                mp::mp_for_each<TypeList>([&](const auto &message) {
                    using Message = std::decay_t<decltype(message)>;
                    if(!found && Message::msgid == frame.msgid) {
                        found = true;
                        //TOOLBOX_DEBUG << "["<<(ptr-begin)<<"] msgid "<<frame.msgid<<" seq "<<frame.seq;
                        Packet<Message, BinaryPacketT> spbpacket(packet); // adds some stuff
                        stream->on_decoded(spbpacket);
                    }
                });
            });
            if(!found) {
                stats_.on_rejected(frame);
                TOOLBOX_DUMP << "SpbDecoder rejected "<<Schema::Venue<<": ["<<(ptr-begin)<<"] unknown msgid "<<frame.msgid;
            }
            ptr += sizeof(Frame) + frame.size;
        }
    }
    SpbDecoderStats& stats() { return stats_; }
private:
    StreamsTuple streams_;
    SpbDecoderStats stats_;
};

}