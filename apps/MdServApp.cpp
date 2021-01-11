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

namespace tb = toolbox;

namespace ft {

class MdServApp {
public:
  using This = MdServApp;
  struct MdOptions {
    int log_level = 0;
    std::string input_format;
    std::string output_format;
    std::string input;
    tb::optional<std::string> output;
  };  
public:
  MdServApp(io::Reactor* reactor)
  : reactor_(reactor)
  {
      This::reactor().state_changed().connect(tb::bind<&This::on_reactor_state_changed>(this));
  }
  tb::Reactor& reactor() { assert(reactor_); return *reactor_; }

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
    if (state == core::State::Stopped) {
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
        if(out_.is_open()) {
          out_ << "sym:'" << ins.symbol()
           //<<", mic:'"<<ins.exchange()
           <<"', "<<bp<<std::endl;
        }
        break;
      }
      default: {
        if(out_.is_open()) {
          out_ << "sym:'" << ins.symbol() << "', " << e << std::endl;  
        }
      }
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
    if(out_.is_open()) {
      out_ << e << std::endl;
    }
  }
  
  using MdClientFactory = std::function<std::unique_ptr<core::IMdClient>(std::string_view venue, io::Reactor *r)>;

  template<typename BinaryPacketT, 
          template<typename ProtocolT> typename ClientTT>
  MdClientFactory make_mdclients_factory () {
    return [this](std::string_view venue, tb::Reactor* r) { 
      if (venue=="SPB_MDBIN") {
          using ClientImpl = ClientTT<spb::SpbUdpProtocol<BinaryPacketT>>;
          return std::unique_ptr<core::IMdClient>(new core::MdClientImpl<ClientImpl>(r));
      } else if(venue=="QSH") {
          using ClientImpl = qsh::QshMdClient;
          return std::unique_ptr<core::IMdClient>(new core::MdClientImpl<ClientImpl>(r));
      } else if(venue=="TB1") {
          using ClientImpl = ClientTT<tbricks::TbricksProtocol<BinaryPacketT>>;
          return std::unique_ptr<core::IMdClient>(new core::MdClientImpl<ClientImpl>(r));
      } else throw std::logic_error("Unknown venue");
    };
  }
  

  // type functions to get concrete MdClient from Protocol, connections types and reactor
  template<typename ProtocolT> 
  using McastMdClient = io::MdClient<
    ProtocolT
  , io::Conn<tb::McastSocket, io::Service<tb::Reactor>>
  , core::State>;  
  template<typename ProtocolT>
  using UdpMdClient = io::MdClient<
    ProtocolT
  , io::Conn<tb::DgramSocket, io::Service<tb::Reactor> >
  , core::State >;
  template<typename ProtocolT>
  using PcapMdClient = io::PcapMdClient<ProtocolT>;

  template<typename ProtocolT> 
  using UdpMdServer = io::MdServer<ProtocolT, io::ServerConn<tb::DgramSocket, io::Service<tb::Reactor> >, tb::DgramSocket>;

  // concrete packet types decoded by Protocols. FIXME: UdpPacket==McastPacket?

  MdClientFactory make_mdclients_factory(std::string_view mode) {
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
    MdClientFactory make_mdclient = make_mdclients_factory(mode);
    TOOLBOX_DUMP<<"make_mdclient venue:"<<venue<<", mode:"<<mode;
    std::unique_ptr<core::IMdClient> client = make_mdclient(venue, reactor_);
    auto& c = *client;
    c.parameters(params);
    c.state_changed().connect(tb:: bind<&This::on_state_changed>(this));
    c
      .ticks(core::StreamTopic::BestPrice)
      .connect(tb::bind<&This::on_tick>(this));
    c
      .instruments(core::StreamTopic::Instrument)
      .connect(tb::bind<&This::on_instrument>(this));    
    return client;
  }

  std::unique_ptr<core::IMdServer> make_mdserver(std::string_view venue, const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;
    using TbricksMdServer = UdpMdServer<tbricks::TbricksProtocol<tb::UdpPacket>>;
    auto server =  std::unique_ptr<core::IMdServer>(new core::MdServerImpl(std::make_unique<TbricksMdServer>(reactor_)));
    auto &s = *server;
    s.parameters(params);
    s.subscribe(StreamTopic::BestPrice)
      .connect(tb::bind<&This::on_subscribe>(this));
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
      case tb::io::State::Stopping: 
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

  void start(core::Parameters& opts) {
            // if not pcap should run reactor
    tb::BasicRunner runner(*reactor_, [this, &opts] {
      return run(opts);
    });
    tb::wait_termination_signal();          
  }
  /// returns true if something has really started and reactor thread is required
  bool run(const core::Parameters& opts) {
      std::string_view mode_force = opts.value_or("mode", std::string_view{});
      std::string output_path = opts.value_or("output", std::string{"/dev/stdout"});
      if(output_path!="/dev/null")
        out_.open(output_path);
      
      std::size_t nstarted = 0;

      for(auto [venue, venue_opts]: opts["servers"]) {
        bool enabled = venue_opts.value_or("enabled", true);
        if(enabled) {
          auto s = make_mdserver(venue, venue_opts);
          auto& server = *s;
          mdservers_.emplace(venue, std::move(s));            
          server.start();
        }
      }
      for(auto [venue, venue_opts]: opts["clients"]) {
        bool enabled = venue_opts.value_or("enabled", true);
        if(enabled) {
          std::string_view mode = venue_opts.value_or("mode", std::string_view{});
          if(!mode_force.empty())
            mode = mode_force;
          auto c = make_mdclient(venue, mode, venue_opts);
          auto& client = *c;
          mdclients_.emplace(venue, std::move(c));
          client.start();
          /*
          client.async_connect(tb::bind([&client](std::error_code ec) {
            TOOLBOX_INFO<<"client connected ";
            core::SubscriptionRequest req;
            req.reset();
            req.request(core::Request::Subscribe);
            req.symbol("MSFT");
            client.async_request(req, nullptr, tb::bind([](ssize_t size, std::error_code ec) {
              if(ec) {
                throw std::system_error(ec, "connect");
              }
            }))
          }));
          */  
        }
      }
      return mode_force!="pcap";
  }
       
  
  int main(int argc, char *argv[]) {
    tb::set_log_level(tb::Log::Dump);
    
    tb::Options parser{"mdserv [OPTIONS] config.json"};
    try {
      core::MutableParameters opts;
      std::string config_file;
      std::string mode;
      bool help = false;

      parser('h', "help", tb::Switch{help}, "print help")
        ('v', "verbose", tb::Value{opts, "verbose", std::uint32_t{}}, "log level")
        ('o', "output", tb::Value{opts, "output", std::string{}}, "output file")
        ('m', "mode", tb::Value{opts, "mode", std::string{}}, "pcap")
        (tb::Value{config_file}.required(), "config.json file")
      .parse(argc, argv);

      if(help) {
        std::cerr << parser << std::endl;
        return EXIT_SUCCESS;
      }
      if(!config_file.empty()) {
        opts.parse_file(config_file);
      }
      start(opts);
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
  tb::MonoTime start_timestamp_;
  core::InstrumentsCache instruments_;
  core::BestPriceCache bestprice_;
  io::Reactor* reactor_{nullptr};
  std::ofstream out_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IMdClient>> mdclients_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IMdServer>> mdservers_;
};

} // namespace ft

int main(int argc, char *argv[]) {
  ft::MdServApp app(ft::io::current_reactor());
  return app.main(argc, argv);
}