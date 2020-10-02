#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/TickMessage.hpp"

namespace ft::spb {

template<typename SchemaT>
class SpbBestPriceStream : public ft::core::Stream<ft::core::TickMessage>
{
public:
    using This=SpbBestPriceStream<SchemaT>;
    using Schema = SchemaT;
    using TypeList = mp::mp_list<typename Schema::SnapshotStart, typename Schema::SnapshotFinish, typename Schema::PriceSnapshot>;
    using SnapshotStart = typename Schema::SnapshotStart;
    using SnapshotFinish = typename Schema::SnapshotStart;
    using PriceSnapshot = typename Schema::PriceSnapshot;
public:
    template<typename DecoderT>
    void subscribe(DecoderT& decoder)
    {
        decoder.connect(tbu::bind<&This::on_snapshot_start>(this));
        decoder.connect(tbu::bind<&This::on_snapshot_finish>(this));
        decoder.connect(tbu::bind<&This::on_price_snapshot>(this));
    }
   
    void on_snapshot_start(const SnapshotStart& e) {
        // raise 
    }
    void on_snapshot_finish(const SnapshotFinish& e) {

    }
    void on_price_snapshot(const PriceSnapshot& e) {

    }
};

}