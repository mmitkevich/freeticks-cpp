#include <boost/mp11.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "ft/capi/ft-types.h"
#include "ft/core/Instrument.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/MdClient.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/io/Connections.hpp"
#include "ft/io/MdClient.hpp"

#include "toolbox/io/PollHandle.hpp"
#include "toolbox/io/Runner.hpp"
#include "toolbox/net/DgramSock.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/sys/Time.hpp"
#include "toolbox/util/Finally.hpp"
#include "toolbox/util/Json.hpp"
#include "toolbox/util/Options.hpp"

#include "ft/pcap/PcapMdClient.hpp"
#include "ft/qsh/QshMdClient.hpp"

#include "ft/spb/SpbProtocol.hpp"

#include "ft/utils/Factory.hpp"
#include "toolbox/util/Json.hpp"
//#include "toolbox/ipc/MagicRingBuffer.hpp"
#include "ft/core/Requests.hpp"

#include "ft/udp/UdpMdServer.hpp"
#include "ft/tbricks/TbricksProtocol.hpp"

namespace ft {

namespace tb = toolbox;
using UdpPacket = io::DgramConn::Packet;
using MdServProtocol = ft::tbricks::TbricksProtocol<UdpPacket>;
using MdServer = udp::UdpMdServer<MdServProtocol>;

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
  using McastPacket = io::McastConn::Packet;
  template<typename ProtocolT> using McastMdClient = io::MdClient<ProtocolT>;
  using PcapPacket = tb::PcapPacket;
  template<typename ProtocolT> using PcapMdClient = pcap::PcapMdClient<ProtocolT>;
  
public:
  MdServApp(core::Reactor* reactor)
  : reactor_(reactor)
  , mdserver_(reactor)
  {
      This::reactor().state_changed().connect(tbu::bind<&This::on_reactor_state_changed>(this));
  }
  tb::Reactor& reactor() { assert(reactor_); return *reactor_; }
  core::IMdClient &mdclient() { return *mdclient_; }
  MdServer &mdserver() { return mdserver_; }
  void gateway(std::unique_ptr<core::IMdClient> mdclient) { mdclient_ = std::move(mdclient); }

  void on_state_changed(core::State state, core::State old_state,
                        core::ExceptionPtr err) {
    TOOLBOX_DEBUG << "gateway state: "<<state;
    if (state == core::State::Failed && err)
      std::cerr << "error: " << err->what() << std::endl;
    if (state == core::State::Stopped) {
      // gateway().report(std::cerr);
      auto elapsed = tb::MonoClock::now() - start_timestamp_;
      auto total_received = mdclient().stats().received();
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
  MdClientFactory make_factory () {
    using SpbProtocol = spb::SpbUdpProtocol<BinaryPacketT>;
    using SpbMdClient = GatewayT<SpbProtocol>;
    using Factory = ftu::Factory<core::IMdClient, core::MdClientProxy>;
  
    return [this](std::string_view venue)->std::unique_ptr<core::IMdClient> { 
      return Factory::make_unique(
        ftu::IdFn{"SPB_MDBIN", [this] { return std::make_unique<SpbMdClient>(reactor_); }},
        ftu::IdFn{"QSH", [this] { return std::make_unique<qsh::QshMdClient>(reactor_); }})
      (venue); 
    };
  }
  MdClientFactory make_factory(const core::Parameters& params) {
    std::string mode = params.value_or("mode", std::string{"mcast"});
    if(mode=="mcast") {
      return make_factory<McastPacket, McastMdClient>();
    } else if(mode=="pcap") {
      return make_factory<PcapPacket, PcapMdClient>();
    } else {
      std::stringstream ss;
      ss<<"Invalid mode "<<mode;
      throw std::runtime_error(ss.str());
    }
  }
  void run(std::string_view venue, const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;

    MdClientFactory factory = make_factory(params);
      
    gateway(factory(venue));
    std::string output_path = params.value_or("output", std::string{"/dev/null"});
    if(output_path!="/dev/null")
      out_.open(output_path);
    start_timestamp_ = tb::MonoClock::now();
    auto& c = mdclient();
    c.url(venue);
    c.parameters(params);
    c.state_changed().connect(tbu:: bind<&This::on_state_changed>(this));
    c
      .ticks(ft::core::streams::BestPrice)
      .connect(tbu::bind<&This::on_tick>(this));
    c
      .instruments(ft::core::streams::Instrument)
      .connect(tbu::bind<&This::on_instrument>(this));
    
    // FIXME: start should be in reactor thread?
    mdserver().start();
    c.start();  // for pcap will read files and stop

    if(c.state() != core::State::Stopped) {
      // if not pcap should run reactor
      tb::Reactor::Runner runner(*reactor_);
      tb::wait_termination_signal();
    }
  }
  
  void on_reactor_state_changed(tb::Scheduler*reactor, tb::io::State state) {
    TOOLBOX_INFO<<"reactor state: "<<state;
    switch(state) {
      case tb::io::State::Stopping: 
        mdclient().stop();
        mdserver().stop();
        break;
      default:
        break;
    }
  }

  int main(int argc, char *argv[]) {
    tb::set_log_level(tb::Log::Debug);
    
    tb::Options parser{"mdserv [OPTIONS] config.json"};
    try {
      core::MutableParameters opts;
      std::string config_file;
      std::string mode;
      bool help = false;

      parser('h', "help", tbu::Switch{help}, "print help")
        ('v', "verbose", tbu::Value{opts["verbose"], std::uint32_t{}}, "log level")
        ('o', "output", tbu::Value{opts["output"], std::string{}}, "output file")
        ('m', "mode", tbu::Value{mode}, "pcap, mcast")
        ('r', "requests", tbu::Value{opts["requests"], std::string{}}, "requests queue")
        (tbu::Value{config_file}.required(), "config.json file")
      .parse(argc, argv);

      if(help) {
        std::cerr << parser << std::endl;
        return EXIT_SUCCESS;
      }
      if(!config_file.empty()) {
        opts.parse_file(config_file);
      }
      if(!mode.empty())
        opts["mode"] = mode;
      run(opts.value<std::string>("venue"), opts);
      return EXIT_SUCCESS;
    } catch (std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << std::endl << parser << std::endl;
      return EXIT_FAILURE;
    }
  }

private:
  MdServer mdserver_;
  tb::MonoTime start_timestamp_;
  std::unique_ptr<core::IMdClient> mdclient_;
  core::InstrumentsCache instruments_;
  core::Reactor* reactor_{nullptr};
  std::ofstream out_;
};

} // namespace ft

int main(int argc, char *argv[]) {
  ft::MdServApp app(ft::core::current_reactor());
  return app.main(argc, argv);
}