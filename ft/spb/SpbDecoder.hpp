#pragma once

#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/pcap/PcapMdGateway.hpp"
#include "ft/utils/Common.hpp"
#include "toolbox/net/Packet.hpp"
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <iomanip>
#include <unordered_map>
#include "ft/spb/SpbFrame.hpp"
#include "toolbox/util/Tuple.hpp"

namespace ft::spb {

class Frame;

class SpbTickStream : public core::SequencedStream<spb::Seq, core::TickStream>
{

};

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
        Base::on_received();
        auto current = values_[frame.msgid];
        values_[frame.msgid] = current+1;
    }
protected:
    MsgStats values_;
};

template<typename MessageT> constexpr bool spb_msgid_v = MessageT::msgid;

template<typename FrameT, typename SchemaT, typename BinaryPacketT, typename StreamsTuple>
class SpbDecoder
{
public:
    using TypeList = typename SchemaT::TypeList;
    using BinaryPacket = BinaryPacketT;
    using Schema = SchemaT;

    template<typename MessageT> 
    using TypedPacket = tbn::TypedPacket<MessageT, BinaryPacket>;
public:
    SpbDecoder(const SpbDecoder&) = delete;
    SpbDecoder(SpbDecoder&&) = delete;
    SpbDecoder(StreamsTuple&& streams)
        : streams_(streams) { }
    
    void on_packet(BinaryPacket packet) {
        const char* begin = packet.data();
        const char* ptr = begin;
        const char* end = begin + packet.size();
        while(ptr < end) {
            const Frame &frame = *reinterpret_cast<const Frame*>(ptr);

            FT_ASSERT(frame.size>=0);
            
            if(frame.size<=0) {
                //TOOLBOX_DEBUG << "["<<(ptr-begin)<<"] invalid size "<<frame.size;
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
                        stats_.on_accepted(frame);
                        stream.on_packet(TypedPacket<Message>(packet));
                    }
                });
            });
            if(!found) {
                //TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<"] unknown msgid "<<frame.msgid;
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