
#include <algorithm>
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

#ifdef USE_PCAP
#include "ft/io/PcapMdClient.hpp"
#endif

#ifdef USE_QSH
#include "ft/qsh/QshMdClient.hpp"
#endif

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
#ifdef USE_CLICKHOUSECPP
#include "ft/io/ClickHouse.hpp"
#endif
#include "toolbox/util/Slot.hpp"

namespace tb = toolbox;

namespace ft::mds {

inline void fail(std::string_view what, std::string_view id="", const char* line="") {
  std::stringstream ss;
  ss << what << " " << id;
  TOOLBOX_ERROR << line <<": " << ss.str();
  throw std::runtime_error(ss.str());
}


class IServiceFactory {
public:
  virtual ~IServiceFactory() = default;
  virtual std::unique_ptr<core::IService> make_service(const core::Parameters& params) = 0;
};

class BasicServiceFactory : public IServiceFactory {
public:
  BasicServiceFactory(io::Service* parent)
  : parent_(parent) {}

  io::Service* parent() { return parent_; }
   void transport(std::string_view val) {
    transport_ = val;
  }

  std::string_view transport(const core::Parameters& params) const {
    if(!transport_.empty())
      return transport_;
    return params.strv("transport");
  }

  std::string_view protocol(const core::Parameters& params) const {
    return params.strv("protocol");
  }
  template<class Iface, class ProxyT> 
  auto make_proxy() {
    return std::unique_ptr<Iface>(new ProxyT(parent()->reactor(), parent()));
  }
protected:
  io::Service* parent_{};
  std::string transport_; // override
};

class MdClientFactory : public BasicServiceFactory {
  using Base = BasicServiceFactory;
public:
  using Base::Base;
 
  std::unique_ptr<core::IService> make_service(const core::Parameters& params) override {
    return std::unique_ptr<core::IService>(make_client(params).release());
  }

  std::unique_ptr<core::IClient> make_client(const core::Parameters& params) {
      std::string_view protocol = this->protocol(params);
      TOOLBOX_INFO<<"make_client, protocol:"<<protocol<<", transport:"<<this->transport(params);
      if (protocol=="SPB_MDB_MCAST") {
         return make_client<spb::SpbProtocol<tb::UdpEndpoint>::Mixin>(params);
      }
    #ifdef USE_QSH 
      else if(protocol=="QSH") {
          using Proxy = core::Proxy<qsh::QshMdClient, core::IClient::Impl>;
          return make_proxy<core::IClient, Proxy>();
      }
    #endif 
      else if(protocol=="TB1") {
          return make_client<tbricks::TbricksProtocol>(params);
      } else {
        fail("unsupported client protocol", protocol, TOOLBOX_FILE_LINE);
        return nullptr;
      }
  }
  template<template<class...> class ProtocolM> 
  std::unique_ptr<core::IClient> make_client(const core::Parameters& params) {
    std::string_view transport = this->transport(params);
    if(transport=="mcast") {
      using Proxy = core::Proxy<io::MdClient<ProtocolM, io::McastConn>, core::IClient::Impl>;
      return make_proxy<core::IClient, Proxy>();
    } else if(transport=="udp") {
      using Proxy = core::Proxy<io::MdClient<ProtocolM, io::DgramConn>, core::IClient::Impl>;
      return make_proxy<core::IClient, Proxy>();
    } 
#ifdef USE_PCAP
    else if(transport=="pcap") {
      using Proxy = core::Proxy<io::PcapMdClient<ProtocolM>, core::IClient::Impl>;
      return make_proxy<core::IClient, Proxy>();
    } 
#endif  
    else {
      fail("unsupported client transport", transport, TOOLBOX_FILE_LINE);
      return nullptr;
    }
  }  

};

class MdServerFactory : public BasicServiceFactory {
  using Base = BasicServiceFactory;
public:
  using Base::Base;

  std::unique_ptr<core::IService> make_service(const core::Parameters& params) override {
    return std::unique_ptr<core::IService>(make_server(params).release());
  }
  
  std::unique_ptr<core::IServer> make_server(const core::Parameters& params) {
    auto protocol = params.strv("protocol");
    TOOLBOX_INFO<<"make_server protocol:"<<protocol<<", transport:"<<this->transport(params);
    if(protocol=="TB1") {
        return make_server<tbricks::TbricksProtocol>(params);
    } else {
      fail("unsupported server protocol", protocol, TOOLBOX_FILE_LINE);
      return nullptr;
    }
  }
  template<template<class, class...>class ProtocolM>
  std::unique_ptr<core::IServer> make_server(const core::Parameters& params) {
    auto transport = params.strv("transport");      
    if(transport=="udp") {
      using MdServer = io::MdServer<
          ProtocolM
        , io::ServerConn<tb::DgramSocket<>> // PeerT
        , tb::DgramSocket<> >; // ServerSocketT 
      using Proxy = core::Proxy<MdServer, core::IServer::Impl>;      
      return make_proxy<core::IServer, Proxy>();
    } else {
      fail("unsupported server transport", transport, TOOLBOX_FILE_LINE);
      return nullptr;
    }
  };
};

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
  void forward(ServicesT& services, const T& e, tb::PendingSlot<std::error_code>& done) {
    std::size_t pending = std::count_if(services.begin(), services.end(), [&](auto& svc) { return svc.second->supports(e.topic()); });
    done.pending(pending);
    for(auto& it: services) {
      auto& svc = it.second;
      if(svc->supports(e.topic())) {
        auto& slot = Stream::Slot<const T&, tb::DoneSlot>::from(svc->slot(e.topic()));
        slot(e, done);
      }
    }
  }

  void on_tick(const core::Tick& e) {
    //auto& ins = instruments_[e.venue_instrument_id()];
    TOOLBOX_DUMP << e;
    if(out_.is_open())
      out_ << e << std::endl;
    forward(mdservers_, e, forward_tick_to_servers_);
    forward(mdsinks_, e, forward_tick_to_sinks_);
  }

  void on_instrument(const core::InstrumentUpdate& e) {
    TOOLBOX_DUMP << e;
    if(out_.is_open())
      out_ << e << std::endl;
    instruments_.update(e);
    forward(mdservers_, e, forward_ins_to_servers_);
    forward(mdsinks_, e, forward_ins_to_sinks_);
  }
  using ClientPtr = std::unique_ptr<core::IClient>;

  ClientPtr make_mdclient(const core::Parameters &params) {
    ClientPtr client = mdclient_factory_.make_client(params);
    auto& c = *client;
    c.parameters(params);
    c.state_changed().connect(tb:: bind<&Self::on_state_changed>(self()));
    c.signal_of<const core::Tick&>(core::StreamTopic::BestPrice)
     .connect(tb::bind<&Self::on_tick>(self()));
    c.signal_of<const core::InstrumentUpdate&>(core::StreamTopic::Instrument)
     .connect(tb::bind<&Self::on_instrument>(self()));    
    return client;
  }

  using ServerPtr = std::unique_ptr<core::IServer>;
  
  ServerPtr make_mdserver(const core::Parameters &params) {
    ServerPtr server = mdserver_factory_.make_server(params);
    auto &s = *server;
    s.parameters(params);
    s.instruments_cache(&instruments_);
    s.subscription()
      .connect(tb::bind([serv=&s](PeerId peer, const core::SubscriptionRequest& req) {
            Self* self = static_cast<Self*>(serv->parent());
            self->on_subscribe(*serv, peer, req);
      }));
    return server;
  }

  std::unique_ptr<core::IService> make_mdsink(const core::Parameters& params) {
    std::unique_ptr<core::IService> svc;
    std::string_view protocol = params.strv("protocol");
  #ifdef USE_CLICKHOUSECPP
    if(protocol=="clickhouse") {
      using ValuesL = mp::mp_list<core::Tick>;
      using Service = io::MultiSinkService<ValuesL, io::ClickHouseService, io::ClickHouseSink>;
      using Proxy = core::Proxy<Service, core::IService::Impl>;
      auto* proxy = new Proxy(&instruments_, reactor(), self());
      svc = std::unique_ptr<core::IService>(proxy);
      return svc;
    } else 
#endif    
    if(protocol=="csv") {
      using ValuesL = mp::mp_list<core::Tick, core::InstrumentUpdate>;
      using Service = io::MultiSinkService<ValuesL, io::Service, io::CsvSink>;
      using Proxy = core::Proxy<Service, core::IService::Impl>;
      auto* proxy = new Proxy(&instruments_, reactor(), self());
      svc = std::unique_ptr<core::IService>(proxy);
      return svc;
    } else {
      fail("protocol not supported", protocol, TOOLBOX_FILE_LINE);
      return nullptr;
    }
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
      
      std::string output_path = params.str("outfile", "");
      
      if(output_path!="") {
        out_.open(output_path, std::ofstream::out);
        //FIXME: back to universal output
        //bestprice_csv_.open(output_path+"-bbo.csv");
        //instrument_csv_.open(output_path+"-ins.csv");
      }
      
      for(auto pa: params["sinks"]) {
        bool enabled = is_service_enabled(pa, mode());
        if(enabled) {
            auto s = make_mdsink(pa);
            auto* sink = s.get();
            sink->parameters(pa);
            mdsinks_.emplace(sink->id(), std::move(s));
            sink->start();
        }
      }

      for(auto pa : params["servers"]) {
        bool enabled = is_service_enabled(pa, mode());
        if(enabled) {
          auto s = make_mdserver(pa);
          auto* server = s.get();
          mdservers_.emplace(server->id(), std::move(s));            
          server->start();
        }
      }
      
      if(mode()=="pcap") {
        mdclient_factory_.transport("pcap");
      }

      for(auto pa : params["clients"]) {
        bool enabled = is_service_enabled(pa, mode());
        if(enabled) {
          auto c = make_mdclient(pa);
          auto* client = c.get();
          mdclients_.emplace(client->id(), std::move(c));
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
    // TODO: migrate to ranges_v3
    assert(async_subscribe_.empty());
    async_subscribe_.set_slot(done);
    for(auto strm_pa: params) {
      for(auto sym_pa: strm_pa["symbols"]) {
        async_subscribe_.inc_pending();
        client.async_write(make_subscription_request(sym_pa.get_string()), async_subscribe_);
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
    bool help = false;
    int log_level=tb::Log::Info;
    std::vector<std::string> pcap_inputs;
    parser('h', "help", tb::Switch{help}, "print help")
      ('v', "verbose", tb::Value{log_level}, "log level")
      ('l', "logfile", tb::Value{parameters(), "logfile", std::string{}}, "log file")
      ('o', "outfile", tb::Value{parameters(), "outfile", std::string{}}, "out file")
      ('m', "mode", tb::Value{mode_}, "pcap, prod, test")
      (tb::Value{config_file}.required(), "config.json file");
    try {
      parser.parse(argc, argv);
      if(help) {
        std::cerr << parser << std::endl;
        return EXIT_SUCCESS;
      }
      //TOOLBOX_CRIT<<"log_level:"<<log_level;
      tb::set_log_level(log_level);
      tb::std_logger_set_file(parameters().strv("logfile", "/dev/stderr"));
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
  void on_forwarded(std::error_code ec) {

  }
private:
  tb::PendingSlot<std::error_code>  forward_tick_to_servers_{tb::bind<&Self::on_forwarded>(self())};
  tb::PendingSlot<std::error_code>  forward_tick_to_sinks_{tb::bind<&Self::on_forwarded>(self())};
  tb::PendingSlot<std::error_code>  forward_ins_to_servers_{tb::bind<&Self::on_forwarded>(self())};
  tb::PendingSlot<std::error_code>  forward_ins_to_sinks_{tb::bind<&Self::on_forwarded>(self())};
  tb::PendingSlot<ssize_t,std::error_code> async_subscribe_;

  core::MutableParameters opts_;
  std::string mode_ {"prod"};
  tb::MonoTime start_timestamp_;
  core::InstrumentsCache instruments_;
  core::BestPriceCache bestprice_;
  // csv sink
  tb::unordered_map<core::Identifier, std::unique_ptr<core::IService>> mdsinks_;
  tb::unordered_map<core::Identifier, std::unique_ptr<core::IClient>> mdclients_;
  tb::unordered_map<core::Identifier, std::unique_ptr<core::IServer>> mdservers_;
  MdClientFactory mdclient_factory_{this};
  MdServerFactory mdserver_factory_{this};
  std::ofstream out_;
};

} // namespace ft::apps::serv

int main(int argc, char *argv[]) {
  ft::mds::App app(ft::io::current_reactor());
  return app.main(argc, argv);
}