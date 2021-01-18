#include <boost/mp11.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_map>

#include "ft/core/Component.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Subscription.hpp"
#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/capi/ft-types.h"
#include "ft/core/Instrument.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/BestPriceCache.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/core/MdServer.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/io/Conn.hpp"
#include "ft/io/MdClient.hpp"
#include "ft/io/Service.hpp"
#include "toolbox/io/McastSocket.hpp"
#include "toolbox/io/Runner.hpp"
#include "toolbox/io/Socket.hpp"
#include "toolbox/net/DgramSock.hpp"
#include "toolbox/net/Endpoint.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Finally.hpp"
#include "toolbox/util/Json.hpp"
#include "toolbox/util/Options.hpp"

#include "ft/io/PcapMdClient.hpp"
#include "ft/qsh/QshMdClient.hpp"

#include "ft/spb/SpbProtocol.hpp"

//#include "ft/utils/Factory.hpp"
#include "toolbox/util/Json.hpp"
//#include "toolbox/ipc/MagicRingBuffer.hpp"
#include "ft/core/Requests.hpp"
#include "toolbox/io/DgramSocket.hpp"
#include "ft/io/MdServer.hpp"
#include "ft/tbricks/TbricksProtocol.hpp"
#include "toolbox/util/RobinHood.hpp"
#include "ft/core/CsvSink.hpp"
#include "toolbox/util/Slot.hpp"

namespace tb = toolbox;

namespace ft::mds {


class App : public io::BasicApp<App> {
  FT_CRTP(App, io::BasicApp<App>);  // boilerplate: Self=App, Base=io::BasicApp<App>, self()->App
public:
  struct MdOptions {
    int log_level = 0;
    std::string input_format;
    std::string output_format;
    std::string input;
    tb::optional<std::string> output;
  };  
public:
  App(io::Reactor* r)
  : Base(r, nullptr) {
     reactor()->state_changed().connect(tb::bind<&Self::on_reactor_state_changed>(self()));
  }

  template<typename T, typename HandlerT>
  auto stat_sum(T result, HandlerT&& h) {
      for(auto& [k, client]: mdclients_) {
          result += h(*client);
      }
      return result;
  }

  void on_state_changed(core::State state, core::State old_state,
                        core::ExceptionPtr err) {
    TOOLBOX_DEBUG << "gateway state: "<<state;
    if (state == core::State::Failed && err)
      std::cerr << "error: " << err->what() << std::endl;
    if (state == core::State::Closed) {
      // gateway().report(std::cerr);
      auto elapsed = tb::MonoClock::now() - start_timestamp_;
      std::size_t total_received = stat_sum(0U, [](auto& cl){ return cl.stats().received(); });
      std::cerr << " read " << total_received << " in "
                   << elapsed.count() / 1e9 << " s"
                   << " at " << (1e3 * total_received / elapsed.count())
                   << " mio/s" << std::endl;
    }
  };

  void on_tick(const core::Tick& e) {
    auto &ins = instruments_[e.venue_instrument_id()];
    switch(e.topic()) {
      case StreamTopic::BestPrice: {
        auto& bp = bestprice_.update(e);
        bestprice_csv_(bp);
        break;
      }
      default: break;
    }
    for(auto& [venue, server]: mdservers_) {
      auto& sink = server->ticks(e.topic());
      sink(e, nullptr);
    }
  }

  void on_instrument(const core::InstrumentUpdate& e) {
    instruments_.update(e);
    for(auto& [venue, server]: mdservers_) {
      auto& sink = server->instruments(e.topic());
      sink(e, nullptr);
    }
    instrument_csv_(e);
  }

  template<class T>
  using Factory = std::function<std::unique_ptr<T>(std::string_view id, core::Component* parent)>;

  template<typename BinaryPacketT, 
          template<typename ProtocolT> typename ClientTT>
  Factory<IMdClient> make_mdclients_factory () {
    return [this](std::string_view venue, core::Component* parent) -> std::unique_ptr<core::IMdClient> { 
      if (venue=="SPB_MDBIN") {
          using ClientImpl = ClientTT<spb::SpbUdpProtocol<BinaryPacketT>>;
          return std::unique_ptr<core::IMdClient>(new core::MdClientImpl<ClientImpl>(reactor(), parent));
      } else if(venue=="QSH") {
          using ClientImpl = qsh::QshMdClient;
          return std::unique_ptr<core::IMdClient>(new core::MdClientImpl<ClientImpl>(reactor(), parent));
      } else if(venue=="TB1") {
          using ClientImpl = ClientTT<tbricks::TbricksProtocol<BinaryPacketT>>;
          return std::unique_ptr<core::IMdClient>(new core::MdClientImpl<ClientImpl>(reactor(), parent));
      } else throw std::logic_error("Unknown venue");
    };
  }
  

  // type functions to get concrete MdClient from Protocol, connections types and reactor
  template<typename ProtocolT> 
  using McastMdClient = io::MdClient<
    ProtocolT
  , io::Conn<tb::McastSocket<>>
  , core::State>;  
  template<typename ProtocolT>
  using UdpMdClient = io::MdClient<
    ProtocolT
  , io::Conn<tb::DgramSocket<>>
  , core::State >;
  template<typename ProtocolT>
  using PcapMdClient = io::PcapMdClient<ProtocolT>;

  template<typename ProtocolT> 
  using UdpMdServer = io::MdServer<ProtocolT, io::ServerConn<tb::DgramSocket<>>, tb::DgramSocket<>>;

  // concrete packet types decoded by Protocols. FIXME: UdpPacket==McastPacket?

  Factory<IMdClient> make_mdclients_factory(std::string_view mode) {
    if(mode=="mcast") {
      return make_mdclients_factory<tb::McastPacket, McastMdClient>();
    } else if(mode=="pcap") {
      return make_mdclients_factory<tb::PcapPacket, PcapMdClient>();
    } else if(mode=="udp") {
      return make_mdclients_factory<tb::UdpPacket, UdpMdClient>();
    } else {
      std::stringstream ss;
      ss<<"Invalid mode "<<mode;
      throw std::runtime_error(ss.str());
    }
  }
  std::unique_ptr<core::IMdClient> make_mdclient(std::string_view venue, std::string_view mode, const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;
    //start_timestamp_ = tb::MonoClock::now();
    Factory<IMdClient> make_mdclient = make_mdclients_factory(mode);
    TOOLBOX_DUMP<<"make_mdclient venue:"<<venue<<", mode:"<<mode;
    std::unique_ptr<core::IMdClient> client = make_mdclient(venue, self());
    auto& c = *client;
    c.parameters(params);
    c.state_changed().connect(tb:: bind<&Self::on_state_changed>(self()));
    c
      .ticks(core::StreamTopic::BestPrice)
      .connect(tb::bind<&Self::on_tick>(self()));
    c
      .instruments(core::StreamTopic::Instrument)
      .connect(tb::bind<&Self::on_instrument>(self()));    
    return client;
  }

  std::unique_ptr<core::IMdServer> make_mdserver(std::string_view venue, const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;
    using TbricksMdServer = UdpMdServer<tbricks::TbricksProtocol<tb::UdpPacket>>;
    auto* r = reactor();
    assert(r);
    auto server =  std::unique_ptr<core::IMdServer>(new core::MdServerImpl<TbricksMdServer>(r, self()));
    auto &s = *server;
    s.parameters(params);
    s.subscribe(StreamTopic::BestPrice)
      .connect(tb::bind<&Self::on_subscribe>(self()));
    return server;
  }

  void on_subscribe( const core::SubscriptionRequest& req) {
    switch(req.request()) {
      case Request::Subscribe: {
        // Instrument->Dest
      } break;
      default: throw std::logic_error("Unknown request");
    }
  }
  

  void on_reactor_state_changed(tb::Scheduler*reactor, tb::io::State state) {
    TOOLBOX_INFO<<"reactor state: "<<state;
    switch(state) {
      case State::PendingClosed: 
        stop();
        break;
      default:
        break;
    }
  }

  void stop() {
    for(auto& [k, s]: mdservers_) {
      s->stop();
    }
    for(auto& [k, c]: mdclients_) {
      c->stop();
    }
  }

  std::string_view mode(std::string_view venue_mode={}) const {
    auto global_mode =  parameters().value_or("mode", std::string_view{});
    if(!global_mode.empty())
      return global_mode;
    return venue_mode;
  }

  void start() {
    assert(reactor());
    auto mod = mode();
    if(mod != "pcap") {
      tb::BasicRunner runner(*reactor(), [this] {
        run();
        return true;
      });
    } else {
      run();
    }
  }

  /// returns true if something has really started and reactor thread is required
  void run() {
      const auto& pa = parameters();
      
      std::string output_path = pa.value_or("output", std::string{"/dev/stdout"});
      
      if(output_path!="/dev/null") {
        bestprice_csv_.open(output_path+"-bbo.csv");
        instrument_csv_.open(output_path+"-ins.csv");
      }
      
      std::size_t nstarted = 0;

      for(auto [venue, venue_opts] : pa["servers"]) {
        bool enabled = venue_opts.value_or("enabled", true);
        if(enabled) {
          auto s = make_mdserver(venue, venue_opts);
          auto& server = *s;
          mdservers_.emplace(venue, std::move(s));            
          server.start();
        }
      }
      for(auto [venue, venue_pa]: pa["clients"]) {
        bool enabled = venue_pa.value_or("enabled", true);
        if(enabled) {
          std::string_view mod = mode(venue_pa.value_or("mode", std::string_view{}));
          auto c = make_mdclient(venue, mod, venue_pa);
          auto* client = c.get();
          TOOLBOX_DUMP<<TOOLBOX_FILE_LINE<<"Client "<<std::hex<<client;
          mdclients_.emplace(venue, std::move(c));
          client->state_changed().connect(tb::bind([client](core::State state, core::State old_state, ExceptionPtr ex) {
            Self* self = static_cast<Self*>(client->parent());
            TOOLBOX_DUMP<<TOOLBOX_FILE_LINE<<"Client "<<std::hex<<client<<" self "<<self<<" state "<<state;
            if(state==core::State::Open) {
              self->on_client_open(*client);
            }
          }));
          client->start();
        }
      }
  }

  void on_client_open(core::IMdClient& client) {
    async_subscribe(client, client.parameters()["subscriptions"], tb::bind([this](ssize_t size, std::error_code ec) {
      if(ec) {
        on_io_error(ec);
      }
    }));
  }

  void async_subscribe(core::IMdClient& client, const core::Parameters& params, tb::SizeSlot done) {
    requests_sent_.set_slot(done);
    for(auto strm_pa: params) {
      for(auto sym_pa: strm_pa["symbols"]) {
        requests_sent_.inc_pending();
        async_subscribe(client, sym_pa.get_string(), requests_sent_.get_slot());
      }
    }
  }

  void async_subscribe(core::IMdClient& client, std::string_view symbol, tb::SizeSlot done) {
    TOOLBOX_INFO << "subscribe symbol:'"<<symbol<<"'";
    core::SubscriptionRequest req;
    req.reset();
    req.request(core::Request::Subscribe);
    req.symbol(symbol);
    client.async_request(req, nullptr, done);  
  }

  void wait() {
    auto mode = parameters().value_or("mode",std::string_view{});
    if(mode == "pcap")
      return; // all done already
    tb::wait_termination_signal();  // main loop is in Reactor thread
  }

  void on_io_error(std::error_code ec) {
    TOOLBOX_ERROR << "io_error "<<std::system_error(ec).what();
  }

  int main(int argc, char *argv[]) {
    tb::set_log_level(tb::Log::Dump);
    
    tb::Options parser{"mdserv [OPTIONS] config.json"};
    try {
      std::string config_file;
      bool help = false;

      parser('h', "help", tb::Switch{help}, "print help")
        ('v', "verbose", tb::Value{parameters(), "verbose", std::uint32_t{}}, "log level")
        ('o', "output", tb::Value{parameters(), "output", std::string{}}, "output file")
        ('m', "mode", tb::Value{parameters(), "mode", std::string{}}, "valid values: pcap, prod, test")
        (tb::Value{config_file}.required(), "config.json file")
      .parse(argc, argv);

      if(help) {
        std::cerr << parser << std::endl;
        return EXIT_SUCCESS;
      }
      if(!config_file.empty()) {
        Base::parameters_.parse_file(config_file);
      }
      start();
      wait();
      return EXIT_SUCCESS;
    } catch (std::system_error &e) {
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    } catch (std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << std::endl << parser << std::endl;
      return EXIT_FAILURE;
    }
  }

private:
  tb::PendingSlot<ssize_t, std::error_code> requests_sent_;

  core::MutableParameters opts_;
  tb::MonoTime start_timestamp_;
  core::InstrumentsCache instruments_;
  core::BestPriceCache bestprice_;
  core::SymbolCsvSink<BestPrice> bestprice_csv_ {instruments_};
  core::CsvSink<InstrumentUpdate> instrument_csv_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IMdClient>> mdclients_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IMdServer>> mdservers_;
};

} // namespace ft::apps::serv

int main(int argc, char *argv[]) {
  ft::mds::App app(ft::io::current_reactor());
  return app.main(argc, argv);
}