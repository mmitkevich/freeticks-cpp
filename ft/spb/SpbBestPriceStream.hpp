#include "ft/spb/SpbSchema.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/TickMessage.hpp"

namespace ft::spb {

template<typename SchenaT> class SpbProtocol;

template<typename SchemaT>
class SpbBestPriceStream : public core::Stream<core::TickMessage>
{
public:
    using This = SpbBestPriceStream<SchemaT>;
    using Schema = SchemaT;
    using TypeList = mp::mp_list<typename Schema::SnapshotStart, typename Schema::SnapshotFinish, typename Schema::PriceSnapshot>;
    using SnapshotStart = typename Schema::SnapshotStart;
    using SnapshotFinish = typename Schema::SnapshotStart;
    using PriceSnapshot = typename Schema::PriceSnapshot;
    using Protocol = SpbProtocol<Schema>;
    using Decoder = typename Protocol::Decoder;
public:
    SpbBestPriceStream(SpbProtocol<SchemaT> & parent)
    : parent_(parent) {
        connect(parent_.decoder());
    }
    ~SpbBestPriceStream() {
        disconnect(parent_.decoder());
    }
    void connect(Decoder& decoder)
    {
        decoder.connect(tbu::bind<&This::on_snapshot_start>(this));
        decoder.connect(tbu::bind<&This::on_snapshot_finish>(this));
        decoder.connect(tbu::bind<&This::on_price_snapshot>(this));
    }
    void disconnect(Decoder& decoder)
    {
        decoder.disconnect(tbu::bind<&This::on_snapshot_start>(this));
        decoder.disconnect(tbu::bind<&This::on_snapshot_finish>(this));
        decoder.disconnect(tbu::bind<&This::on_price_snapshot>(this));
    }  
    void on_snapshot_start(const SnapshotStart& e) {
        
    }
    void on_snapshot_finish(const SnapshotFinish& e) {
        
    }
    void on_price_snapshot(const PriceSnapshot& e) {
        //TOOLBOX_INFO << e;
        total_count_++;
        if(total_count_%10000==0) {
            TOOLBOX_INFO << "#"<<total_count_<<": "<<e;
        }
    }
private:
    SpbProtocol<SchemaT>& parent_;
    std::size_t total_count_{0};
};

}