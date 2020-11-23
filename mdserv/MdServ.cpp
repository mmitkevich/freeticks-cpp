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
#include "ft/core/MdGateway.hpp"
#include "ft/core/Parameters.hpp"
#include "ft/core/Executor.hpp"

#include "ft/mcast/McastMdGateway.hpp"

#include "toolbox/io/Runner.hpp"
#include "toolbox/net/EndpointFilter.hpp"
#include "toolbox/sys/Log.hpp"
#include "toolbox/util/Finally.hpp"
#include "toolbox/util/Json.hpp"
#include "toolbox/util/Options.hpp"

#include "ft/pcap/PcapMdGateway.hpp"
#include "ft/qsh/QshMdGateway.hpp"

#include "ft/spb/SpbProtocol.hpp"

#include "ft/utils/Factory.hpp"
#include "toolbox/util/Json.hpp"
#include "toolbox/ipc/MagicRingBuffer.hpp"
#include "ft/core/Request.hpp"

namespace ft {

namespace tbu = toolbox::util;
namespace tbs = toolbox::sys;
namespace tb = toolbox;
namespace tbj = toolbox::json;


template<typename SockT>
class MdRequests {
public:
  MdRequests() {}
  void url(std::string_view url) { url_ = url; }
  void start() {
    sock_ = std::make_unique<SockT>();
  }
  void stop() {
    sock_.reset(nullptr);
  }
  SockT& sock() {
    assert(sock_!=nullptr);
    return *sock_;
  }

private:
  std::unique_ptr<SockT> sock_;
  std::string url_;
};
class MdServ {
public:
  using This = MdServ;
  struct MdOptions {
    int log_level = 0;
    std::string input_format;
    std::string output_format;
    std::string input;
    tb::optional<std::string> output;
  };
  using McastPacket = mcast::McastDgram;
  template<typename ProtocolT> using McastMdGateway = mcast::McastMdGateway<ProtocolT, core::Executor>;
  using PcapPacket = tb::PcapPacket;
  template<typename ProtocolT> using PcapMdGateway = pcap::PcapMdGateway<ProtocolT, core::Executor>;
  
public:
  MdServ(core::Reactor& reactor)
  : reactor_(reactor)
  {
      reactor_.state_changed().connect(tbu::bind<&This::on_reactor_state_changed>(this));
  }

  core::IMdGateway &gateway() { return *gateway_; }
  void gateway(std::unique_ptr<core::IMdGateway> gw) { gateway_ = std::move(gw); }

  void on_state_changed(core::State state, core::State old_state,
                        core::ExceptionPtr err) {
    TOOLBOX_DEBUG << "gateway state: "<<state;
    if (state == core::State::Failed && err)
      std::cerr << "error: " << err->what() << std::endl;
    if (state == core::State::Stopped) {
      // gateway().report(std::cerr);
      auto elapsed = tbs::MonoClock::now() - start_timestamp_;
      auto total_received = gateway().stats().received();
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
  
  using MdGwFactory = std::function<std::unique_ptr<core::IMdGateway>(std::string_view venue)>;

  template<typename BinaryPacketT, template<typename ProtocolT> typename GatewayT>
  MdGwFactory make_factory () {
    using SpbProtocol = spb::SpbUdpProtocol<BinaryPacketT>;
    using SpbMdGateway = GatewayT<SpbProtocol>;
    using Factory = ftu::Factory<core::IMdGateway, core::MdGateway>;
  
    return [this](std::string_view venue)->std::unique_ptr<core::IMdGateway> { 
      return Factory::make_unique(
        ftu::IdFn{"SPB_MDBIN", [this] { return std::make_unique<SpbMdGateway>(&reactor_); }},
        ftu::IdFn{"QSH", [this] { return std::make_unique<qsh::QshMdGateway>(&reactor_); }})
      (venue); 
    };
  }
  MdGwFactory make_factory(const core::Parameters& params) {
    std::string mode = params.value_or("mode", std::string{"mcast"});
    if(mode=="mcast") {
      return make_factory<McastPacket, McastMdGateway>();
    } else if(mode=="pcap") {
      return make_factory<PcapPacket, PcapMdGateway>();
    } else {
      std::stringstream ss;
      ss<<"Invalid mode "<<mode;
      throw std::runtime_error(ss.str());
    }
  }
  void run(std::string_view venue, const core::Parameters &params) {
    //TOOLBOX_DEBUG<<"run venue:'"<<venue<<"', params:"<<params;

    MdGwFactory factory = make_factory(params);
      
    gateway(factory(venue));
    std::string output_path = params.value_or("output", std::string{"/dev/null"});
    if(output_path!="/dev/null")
      out_.open(output_path);
    start_timestamp_ = tbs::MonoClock::now();
    gateway().url(venue);
    gateway().parameters(params);
    gateway().state_changed().connect(tbu:: bind<&This::on_state_changed>(this));
    gateway()
        .ticks(ft::core::streams::BestPrice)
        .connect(tbu::bind<&This::on_tick>(this));
    gateway()
        .instruments(ft::core::streams::Instrument)
        .connect(tbu::bind<&This::on_instrument>(this));

    gateway().start();
    if(gateway().state()!=core::State::Stopped) {
      tb::Reactor::Runner runner(reactor_);
      tb::wait_termination_signal();
    }
  }
  
  void on_reactor_state_changed(tb::Scheduler*reactor, tb::io::State state) {
    TOOLBOX_INFO<<"reactor state: "<<state;
    switch(state) {
      case tb::io::State::Stopping: 
        gateway().stop();
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
  MdRequests<tb::MagicRingBuffer> requests_;
  tb::MonoTime start_timestamp_;
  std::unique_ptr<core::IMdGateway> gateway_;
  core::InstrumentsCache instruments_;
  core::Reactor& reactor_;
  std::ofstream out_;
};

} // namespace ft

int main(int argc, char *argv[]) {
  ft::MdServ app(ft::core::current_reactor());
  return app.main(argc, argv);
}