#pragma once

#include "ft/pcap/PcapMdGateway.hpp"
#include "ft/utils/Common.hpp"
#include "toolbox/net/Packet.hpp"
#include <iomanip>
#include <unordered_map>
#include "ft/spb/SpbFrame.hpp"

namespace ft::spb {

class Frame;

class SpbDecoderStats: public pcap::BasicStats<SpbDecoderStats> {
public:
    using Base = pcap::BasicStats<SpbDecoderStats>;
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

template<typename FrameT, typename SchemaT, typename BinaryPacketT>
class SpbDecoder
{
public:
    using TypeList = typename SchemaT::TypeList;
    using BinaryPacket = BinaryPacketT;
    using Schema = SchemaT;

    template<typename MessageT> 
    using TypedPacket = tbn::TypedPacket<MessageT, BinaryPacket>;

    // signals tuple
    using TypedPacketSignals = ftu::SignalTuple<mp::mp_transform<TypedPacket, TypeList> >;

public:
    SpbDecoder(const SpbDecoder&) = delete;
    SpbDecoder(SpbDecoder&&) = delete;
    SpbDecoder(){}
    TypedPacketSignals& signals() { return signals_; }
    void on_packet(BinaryPacket packet) {
        const char* begin = packet.data();
        const char* ptr = begin;
        const char* end = begin + packet.size();
        while(ptr < end) {
            const Frame &frame = *reinterpret_cast<const Frame*>(ptr);

            FT_ASSERT(frame.size>=0);
            
            if(frame.size<=0) {
                TOOLBOX_DEBUG << "["<<(ptr-begin)<<"] invalid size "<<frame.size;
                break;
            }

            stats_.on_received(frame);

            bool found = false;
            signals_.for_each([&](auto &cb) {
                using CB = std::decay_t<decltype(cb)>;
                using Message = typename CB::value_type::value_type;
                if(!found && Message::Base::msgid == frame.msgid) {
                    found = true;
                    stats_.on_accepted(frame);
                    //TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<".."<<(ptr+frame.size-data.begin())<<"] msgid "<<frame.msgid << (cb ? "":" - skipped");
                    if(cb) {
                        cb(packet);
                    }
                }
            });
            if(!found) {
                //TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<"] unknown msgid "<<frame.msgid;
            }
            ptr += sizeof(Frame) + frame.size;
        }
    }
    SpbDecoderStats& stats() { return stats_; }
private:
    TypedPacketSignals signals_;
    SpbDecoderStats stats_;
};

}