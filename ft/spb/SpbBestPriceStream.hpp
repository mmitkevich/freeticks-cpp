#pragma once
#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "toolbox/net/Packet.hpp"
#include "ft/spb/SpbSchema.hpp"
#include "ft/spb/SpbDecoder.hpp"
namespace ft::spb {
namespace tbn = toolbox::net;


template<typename DecoderT>
class SpbBestPriceStream : public core::TickStream
{
public:
    using Base = core::TickStream;
    using This = SpbBestPriceStream<DecoderT>;
    using Decoder = DecoderT;
    using Schema = typename Decoder::Schema;
    // supported messages
    using SnapshotStart = typename Schema::SnapshotStart;
    using SnapshotFinish = typename Schema::SnapshotStart;
    using PriceSnapshot = typename Schema::PriceSnapshot;
    // list of supported messages
    using TypeList = mp::mp_list<SnapshotStart, SnapshotFinish, PriceSnapshot>;
    // packet type meta function
    template<typename MessageT>
    using TypedPacket = typename Decoder::template TypedPacket<MessageT>;
public:
    SpbBestPriceStream(Decoder& decoder)
    :   decoder_(decoder) {
        connect();
    }
    ~SpbBestPriceStream() {
        disconnect();
    }
    Decoder& decoder() { return decoder_; } 
protected:
    void connect() {
        decoder_.signals().connect(tbu::bind<&This::on_snapshot_start>(this));
        decoder_.signals().connect(tbu::bind<&This::on_snapshot_finish>(this));
        decoder_.signals().connect(tbu::bind<&This::on_price_snapshot>(this));
    }
    void disconnect() {
        decoder_.signals().disconnect(tbu::bind<&This::on_snapshot_start>(this));
        decoder_.signals().disconnect(tbu::bind<&This::on_snapshot_finish>(this));
        decoder_.signals().disconnect(tbu::bind<&This::on_price_snapshot>(this));
    }
protected:
    void on_snapshot_start(TypedPacket<SnapshotStart> e) { }
    void on_snapshot_finish(TypedPacket<SnapshotFinish> e) { }
    void on_price_snapshot(TypedPacket<PriceSnapshot> e) {
        //TOOLBOX_INFO << e;
        stats().total_received++;
        if(stats().total_received % 100'000 == 0) {
            TOOLBOX_INFO << "total_received:"<<stats().total_received<<", "<<e;
        }
    }
private:
    Decoder& decoder_;
};

}