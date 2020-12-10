#include <boost/mp11.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include "ft/capi/ft-types.h"
#include "ft/core/Instrument.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/core/MdServer.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/io/Connection.hpp"
#include "ft/io/MdClient.hpp"

#include "toolbox/io/PollHandle.hpp"
#include "toolbox/io/Reactor.hpp"
#include "toolbox/io/Runner.hpp"
#include "toolbox/net/DgramSock.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Finally.hpp"
#include "toolbox/util/Json.hpp"
#include "toolbox/util/Options.hpp"

#include "ft/io/PcapMdClient.hpp"
#include "ft/qsh/QshMdClient.hpp"

#include "ft/spb/SpbProtocol.hpp"

#include "ft/utils/Factory.hpp"
#include "toolbox/util/Json.hpp"
//#include "toolbox/ipc/MagicRingBuffer.hpp"
#include "ft/core/Requests.hpp"

#include "ft/io/MdServer.hpp"
#include "ft/tbricks/TbricksProtocol.hpp"
#include "toolbox/util/RobinHood.hpp"

namespace ft {

namespace tb = toolbox;
template<typename ProtocolT> 
using UdpMdServer = io::BasicMdServer<ProtocolT, io::DgramConn, tb::Reactor>;

template<typename ProtocolT> 
using McastMdClient = io::BasicMdClient<ProtocolT, io::McastConn, tb::Reactor>;  
template<typename ProtocolT>
using UdpMdClient = io::BasicMdClient<ProtocolT, io::DgramConn, tb::Reactor>;
template<typename ProtocolT>
using PcapMdClient = io::PcapMdClient<ProtocolT>;

using UdpPacket = io::DgramConn::Packet;
using McastPacket = io::McastConn::Packet;
using PcapPacket = tb::PcapDevice::Packet;

using TbricksProtocol = ft::tbricks::TbricksProtocol<UdpPacket>;


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
  MdServApp(core::Reactor* reactor)
  : reactor_(reactor)
  {
      This::reactor().state_changed().connect(tbu::bind<&This::on_reactor_state_changed>(this));
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

  void on_tick(const core::Tick &e) {
    // TOOLBOX_INFO << e;
    auto &ins = instruments_[e.venue_instrument_id()];
    if(out_.is_open())
      out_ << "m:tick,sym:'" << ins.venue_symbol() << "'," << e << std::endl;
  }

  void on_instrument(const core::VenueInstrument &e) {
    // TOOLBOX_INFO  << e;
    instruments_.update(e);
    if(out_.is_open()) {
      out_ << "m:ins,"<<e<<std::endl;
    }
  }
  
  using MdClientFactory = std::function<std::unique_ptr<core::IMdClient>(std::string_view venue)>;

  template<typename BinaryPacketT, template<typename ProtocolT> typename GatewayT>
  MdClientFactory make_mdclients_factory () {
    using SpbMdClient = GatewayT<spb::SpbUdpProtocol<BinaryPacketT>>;
    using TbricksMdClient = GatewayT<tbricks::TbricksProtocol<BinaryPacketT>>;
    using Factory = ftu::Factory<core::IMdClient, core::MdClientImpl>;
  
    return [this](std::string_view venue)->std::unique_ptr<core::IMdClient> { 
      return Factory::make_unique(
         ftu::IdFn{"SPB_MDBIN", [this] { return std::make_unique<SpbMdClient>(reactor_); }}
        ,ftu::IdFn{"QSH", [this] { return std::make_unique<qsh::QshMdClient>(reactor_); }}
        ,ftu::IdFn{"TB1", [this] { return std::make_unique<TbricksMdClient>(reactor_); }}
      )(venue); 
    };
  }
  MdClientFactory make_mdclients_factory(const core::Parameters& params) {
    std::string mode = params.value_or("mode", std::string{"mcast"});
    if(mode=="mcast") {
      return make_mdclients_factory<McastPacket, McastMdClient>();
    } else if(mode=="pcap") {
      return make_mdclients_factory<PcapPacket, PcapMdClient>();
    } else if(mode=="udp") {
      return make_mdclients_factory<UdpPacket, UdpMdClient>();
    } else {
      std::stringstream ss;
      ss<<"Invalid mode "<<mode;
      throw std::runtime_error(ss.str());
    }
  }
  std::unique_ptr<core::IMdClient> make_mdclient(std::string_view venue, const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;

    MdClientFactory factory = make_mdclients_factory(params);
    std::unique_ptr<core::IMdClient> client = factory(venue);
    start_timestamp_ = tb::MonoClock::now();
    auto& c = *client;
    c.url(venue);
    c.parameters(params);
    c.state_changed().connect(tbu:: bind<&This::on_state_changed>(this));
    c
      .ticks(ft::core::streams::BestPrice)
      .connect(tbu::bind<&This::on_tick>(this));
    c
      .instruments(ft::core::streams::Instrument)
      .connect(tbu::bind<&This::on_instrument>(this));    
    return client;
  }

  std::unique_ptr<core::IMdServer> make_mdserver(std::string_view venue, const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;
    using TbricksMdServer = UdpMdServer<tbricks::TbricksProtocol<UdpPacket>>;
    return std::unique_ptr<core::IMdServer>(new core::MdServerImpl(std::make_unique<TbricksMdServer>(reactor_)));
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

  /// returns true if something has really started and reactor thread is required
  bool start(core::Parameters& opts) {
      std::string output_path = opts.value_or("output", std::string{"/dev/null"});
      if(output_path!="/dev/null")
        out_.open(output_path);
      
      std::size_t nstarted = 0;

      for(auto [venue, venue_opts]: opts["servers"]) {
        auto server = make_mdserver(venue, venue_opts);
        server->start(); 
        if(server->state()!=core::State::Stopped)
          nstarted++;
        mdservers_.emplace(venue, std::move(server));
      }

      for(auto [venue, venue_opts]: opts["clients"]) {
        auto client = make_mdclient(venue, venue_opts);
        client->start(); 
        if(client->state()!=core::State::Stopped)
          nstarted++;
        mdclients_.emplace(venue, std::move(client));
      }
      return nstarted>0; 
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
        ('v', "verbose", tb::Value{opts["verbose"], std::uint32_t{}}, "log level")
        ('o', "output", tb::Value{opts["output"], std::string{}}, "output file")
        (tbu::Value{config_file}.required(), "config.json file")
      .parse(argc, argv);

      if(help) {
        std::cerr << parser << std::endl;
        return EXIT_SUCCESS;
      }
      if(!config_file.empty()) {
        opts.parse_file(config_file);
      }
    
      if(start(opts)) {
        // if not pcap should run reactor
        tb::Reactor::Runner runner(*reactor_);
        tb::wait_termination_signal();
      }

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
  core::Reactor* reactor_{nullptr};
  std::ofstream out_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IMdClient>> mdclients_;
  tb::RobinFlatMap<std::string, std::unique_ptr<core::IMdServer>> mdservers_;
};

} // namespace ft

int main(int argc, char *argv[]) {
  ft::MdServApp app(ft::core::current_reactor());
  return app.main(argc, argv);
}