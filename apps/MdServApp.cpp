
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include "ft/utils/Common.hpp"
#include "ft/core/Component.hpp"
#include "ft/core/Service.hpp"
#include "ft/core/Stream.hpp"
#include "ft/core/Tick.hpp"
#include "ft/capi/ft-types.h"
#include "ft/core/Instrument.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/BestPriceCache.hpp"
#include "ft/core/Client.hpp"
#include "ft/core/Server.hpp"
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
#include "toolbox/net/Protocol.hpp"
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
#include "ft/io/Csv.hpp"
#include "ft/io/ClickHouse.hpp"
#include "toolbox/util/Slot.hpp"

namespace tb = toolbox;

namespace ft::mds {


class App : public io::BasicApp<App> {
  using Base = io::BasicApp<App>;
  using Self = App;
  FT_SELF(Self);
public:
  enum class MdMode {
      Real,
      Pcap
  };
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
      for(auto& it: mdclients_) {
          result += h(*it.second);
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
      std::size_t total_received = stat_sum(0U, [](auto& cl){ return cl.signal(core::StreamTopic::BestPrice).stats().received(); });
      std::cerr << " read " << total_received << " in "
                   << elapsed.count() / 1e9 << " s"
                   << " at " << (1e3 * total_received / elapsed.count())
                   << " mio/s" << std::endl;
    }
  };

  template<class ServicesT, class T>
  void forward(ServicesT& services, const T& e, tb::DoneSlot done) {
    for(auto& it: services) {
      auto& svc = it.second;
      auto& slot = Stream::Slot<const T&, tb::DoneSlot>::from(svc->slot(e.topic()));
      slot(e, done);
    }
  }

  void on_tick(const core::Tick& e) {
    //auto& ins = instruments_[e.venue_instrument_id()];
    TOOLBOX_DUMP << e;
    forward(mdservers_, e, nullptr);
    forward(mdsinks_, e, nullptr);
  }

  void on_instrument(const core::InstrumentUpdate& e) {
    TOOLBOX_DUMP << e;
    instruments_.update(e);
    //forward(mdservers_, e, nullptr);
    //instrument_csv_(e);
  }

  template<class T>
  using Factory = std::function<std::unique_ptr<T>(std::string_view id, core::Component* parent)>;

  template<template<template<class...> class ProtocolM> class ClientM, class EndpointT>
  Factory<IClient> make_mdclients_factory () {
    return [this](std::string_view venue, core::Component* parent) -> std::unique_ptr<core::IClient> { 
      if (venue=="SPB_MDBIN") {
          using ClientImpl = ClientM<spb::SpbProtocol<EndpointT>::template Mixin>;
          using Proxy = core::Proxy<ClientImpl, core::IClient::Impl>;
          return std::unique_ptr<core::IClient>(new Proxy(reactor(), parent));
      } else if(venue=="QSH") {
          using ClientImpl = qsh::QshMdClient;
          using Proxy = core::Proxy<ClientImpl, core::IClient::Impl>;
          return std::unique_ptr<core::IClient>(new Proxy(reactor(), parent));
      } else if(venue=="TB1") {
          using ClientImpl = ClientM<tbricks::TbricksProtocol>;
          using Proxy = core::Proxy<ClientImpl, core::IClient::Impl>;
          return std::unique_ptr<core::IClient>(new Proxy(reactor(), parent));
      } else {
        Self::fail("venue not supported", venue, TOOLBOX_FILE_LINE);
        return nullptr;
      }
    };
  }
  
  // type functions to get concrete MdClient from Protocol, connections types and reactor
  template<template<class...> class ProtocolM> 
  using McastMdClient = io::MdClient<ProtocolM, io::McastConn>;  
  template<template<class...> class ProtocolM>
  using UdpMdClient = io::MdClient<ProtocolM, io::DgramConn>;
  template<template<class...> class ProtocolM>
  using PcapMdClient = io::PcapMdClient<ProtocolM>;

  template<template<class> class ProtocolT> 
  using UdpMdServer = io::MultiServer<mp::mp_list<
    io::MdServer<ProtocolT, io::ServerConn<tb::DgramSocket<>>, tb::DgramSocket<>>
  >>;

  // concrete packet types decoded by Protocols. FIXME: UdpPacket==McastPacket?

  Factory<IClient> make_mdclients_factory(std::string_view transport) {
    if(transport=="mcast") {
      return make_mdclients_factory<McastMdClient, tb::McastEndpoint>();
    } else if(transport=="pcap") {
      return make_mdclients_factory<PcapMdClient, tb::IpEndpoint>();
    } else if(transport=="udp") {
      return make_mdclients_factory<UdpMdClient, tb::UdpEndpoint>();
    } else {
      Self::fail("protocol not supported", transport, TOOLBOX_FILE_LINE);
      return nullptr;
    }
  }
  std::unique_ptr<core::IClient> make_mdclient(std::string_view venue,const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;
    //start_timestamp_ = tb::MonoClock::now();
    auto transport = params.strv("transport");
    if(mode()=="pcap")
      transport = "pcap";
    Factory<IClient> make_mdclient = make_mdclients_factory(transport);
    TOOLBOX_DUMP<<"make_mdclient venue:"<<venue<<", transport:"<<transport;
    std::unique_ptr<core::IClient> client = make_mdclient(venue, self());
    auto& c = *client;
    c.parameters(params);
    c.state_changed().connect(tb:: bind<&Self::on_state_changed>(self()));
    c.signal_of<const core::Tick&>(core::StreamTopic::BestPrice)
     .connect(tb::bind<&Self::on_tick>(self()));
    c.signal_of<const core::InstrumentUpdate&>(core::StreamTopic::Instrument)
     .connect(tb::bind<&Self::on_instrument>(self()));    
    TOOLBOX_INFO << "created md client venue:'"<<venue<<"', transport:'"<<transport<<"'";
    return client;
  }

  std::unique_ptr<core::IServer> make_mdserver(std::string_view venue, const core::Parameters &params) {
    std::unique_ptr<core::IServer> server;
    if(venue=="TB1") {
      using TbricksMdServer = UdpMdServer<tbricks::TbricksProtocol>;
      auto* r = reactor();
      assert(r);
      /// implement IServer using TbricksMdServer class
      using Proxy = core::Proxy<TbricksMdServer, core::IServer::Impl>;
      server =  std::unique_ptr<core::IServer>(new Proxy(r, self()));
      auto &s = *server;
      s.parameters(params);
      s.subscription()
        .connect(tb::bind([serv=&s](PeerId peer, const core::SubscriptionRequest& req) {
            Self* self = static_cast<Self*>(serv->parent());
            self->on_subscribe(*serv, peer, req);
        }));
    } else {
      Self::fail("venue not supported", venue, TOOLBOX_FILE_LINE);
    }
    TOOLBOX_INFO << "created md server '"<<venue<<"'";
    return server;
  }
  std::unique_ptr<core::IService> make_mdsink(std::string_view venue, const core::Parameters& params) {
    std::unique_ptr<core::IService> sink;
    if(venue=="ClickHouse") {
        using ClickHouseSinksL = mp::mp_list<core::Tick>;
        using ClickHouseService = io::ClickHouseService<ClickHouseSinksL>;
        using Proxy = core::Proxy<ClickHouseService, core::IService::Impl>;
        auto* proxy = new Proxy(&instruments_, reactor(), self());
        sink = std::unique_ptr<core::IService>(proxy);
        sink->parameters(params);
        return sink;
    } else {
      Self::fail("venue not supported", venue, TOOLBOX_FILE_LINE);
    }
    TOOLBOX_INFO << "created md sink '"<<venue<<"'";
    return sink;
  }
  static void fail(std::string_view what, std::string_view id="", const char* line="") {
    std::stringstream ss;
    ss << what << " " << id;
    TOOLBOX_ERROR << line <<": " << ss.str();
    throw std::runtime_error(ss.str());
  }
  void on_subscribe(core::IServer& serv, PeerId peer, const core::SubscriptionRequest& req) {
    TOOLBOX_INFO << "subscribe:"<<req;
    switch(req.request()) {
      case Request::Subscribe: {
        
      } break;
      default: {
        TOOLBOX_ERROR<<"request not supported: "<<req.request();
      }
    }
  }
  

  void on_reactor_state_changed(tb::Reactor* reactor, tb::io::State state) {
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
    for(auto& it: mdservers_) {
      it.second->stop();
    }
    for(auto& it: mdclients_) {
      it.second->stop();
    }
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
  std::unordered_set<std::string> get_modeset(const core::Parameters& params) {
    std::unordered_set<std::string> modeset;
    for(auto it: params) {
      modeset.insert(std::string(it.get_string()));
    }
    return modeset;
  }

  bool is_service_enabled(const core::Parameters& params, std::string mode) {
    auto modes = get_modeset(params["enable"]);
    return modes.find(mode)!=modes.end();
  }
  std::string mode() const { return mode_; }

  /// returns true if something has really started and reactor thread is required
  void run() {
      const auto& params = parameters();
      
      std::string output_path = params.str("output", "/dev/stdout");
      
      if(output_path!="/dev/null") {
        //FIXME: back to universal output
        //bestprice_csv_.open(output_path+"-bbo.csv");
        //instrument_csv_.open(output_path+"-ins.csv");
      }
      
      for(auto [id, pa]: params["sinks"]) {
        bool enabled = is_service_enabled(pa, mode());
        if(enabled) {
            auto s = make_mdsink(id, pa);
            auto& sink = *s;
            mdsinks_.emplace(id, std::move(s));
            sink.start();
        }
      }

      for(auto [id, pa] : params["servers"]) {
        bool enabled = is_service_enabled(pa, mode());
        if(enabled) {
          auto s = make_mdserver(id, pa);
          auto& server = *s;
          mdservers_.emplace(id, std::move(s));            
          server.start();
        }
      }
      for(auto [id, pa]: params["clients"]) {
        bool enabled = is_service_enabled(pa, mode());
        if(enabled) {
          auto c = make_mdclient(id, pa);
          auto* client = c.get();
          mdclients_.emplace(id, std::move(c));
          client->state_changed().connect(tb::bind([client](core::State state, core::State old_state, ExceptionPtr ex) {
            Self* self = static_cast<Self*>(client->parent());
            if(state==core::State::Open) {
              self->on_client_open(*client);
            }
          }));
          client->start();
        }
      }
  }

  void on_client_open(core::IClient& client) {
    async_subscribe(client, client.parameters()["subscriptions"], tb::bind([this](ssize_t size, std::error_code ec) {
      if(ec) {
        on_io_error(ec);
      }
    }));
  }

  /// will call done when all subscribed
  void async_subscribe(core::IClient& client, const core::Parameters& params, tb::SizeSlot done) {
    subscribe_done_.pending(0);
    subscribe_done_.set_slot(done);
    for(auto strm_pa: params) {
      for(auto sym_pa: strm_pa["symbols"]) {
        subscribe_done_.inc_pending();
        auto req = make_subscription_request(sym_pa.get_string());
        // will serialize
        client.async_write(req, subscribe_done_.get_slot());
      }
    }
  }

  core::SubscriptionRequest make_subscription_request(std::string_view symbol) {
    //TOOLBOX_INFO << "subscribe symbol:'"<<symbol<<"'";
    core::SubscriptionRequest req;
    req.reset();
    req.request(core::Request::Subscribe);
    req.symbol(symbol);
    return req;
  }

  void wait() {
    if(mode() == "pcap")
      return; // all done already
    tb::wait_termination_signal();  // main loop is in Reactor thread
  }

  void on_io_error(std::error_code ec) {
    TOOLBOX_ERROR << "io_error "<<std::system_error(ec).what();
  }

  int main(int argc, char *argv[]) {
    tb::Options parser{"mdserv [OPTIONS] config.json"};
    std::string config_file;
    std::vector<std::string> pcaps;
    bool help = false;
    int log_level=tb::Log::Info;
    parser('h', "help", tb::Switch{help}, "print help")
      ('v', "verbose", tb::Value{log_level}, "log level")
      ('o', "output", tb::Value{parameters(), "output", std::string{}}, "output file")
      ('m', "mode", tb::Value{mode_}, "pcap, prod, test")
      (tb::Value{config_file}.required(), "config.json file");
    try {
      parser.parse(argc, argv);
      if(help) {
        std::cerr << parser << std::endl;
        return EXIT_SUCCESS;
      }
      TOOLBOX_CRIT<<"log_level:"<<log_level;
      tb::set_log_level(log_level);
      if(!config_file.empty()) {
        Base::parameters_.parse_file(config_file);
      }
    } catch(std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << std::endl << parser << std::endl;
      return EXIT_FAILURE;
    }
    try {
      start();
      wait();
      return EXIT_SUCCESS;
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

private:
  tb::PendingSlot<ssize_t, std::error_code> subscribe_done_;
  core::MutableParameters opts_;
  std::string mode_ {"prod"};
  tb::MonoTime start_timestamp_;
  core::InstrumentsCache instruments_;
  core::BestPriceCache bestprice_;
  // csv sink
  //core::SymbolCsvSink<BestPrice> bestprice_csv_ {instruments_};
  //core::CsvSink<InstrumentUpdate> instrument_csv_;
  tb::unordered_map<std::string, std::unique_ptr<core::IService>> mdsinks_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IClient>> mdclients_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IServer>> mdservers_;
};

} // namespace ft::apps::serv

int main(int argc, char *argv[]) {
  ft::mds::App app(ft::io::current_reactor());
  return app.main(argc, argv);
}