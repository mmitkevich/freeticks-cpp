#include <boost/mp11.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "ft/core/Instrument.hpp"
#include "ft/core/InstrumentsCache.hpp"
#include "ft/core/MdGateway.hpp"
#include "ft/core/Parameterized.hpp"
#include "ft/io/Reactor.hpp"

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


namespace ft {

namespace tbu = toolbox::util;
namespace tbs = toolbox::sys;
namespace tb = toolbox;
namespace tbj = toolbox::json;

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

public:
  MdServ(io::Reactor& reactor)
  : reactor_(reactor)
  {
      reactor_.state_changed().connect(tbu::bind<&This::on_reactor_state_changed>(this));
  }

  core::IMdGateway &gateway() { return *gateway_; }
  void gateway(std::unique_ptr<core::IMdGateway> gw) { gateway_ = std::move(gw); }

  void on_state_changed(core::GatewayState state, core::GatewayState old_state,
                        core::ExceptionPtr err) {
    if (state == core::GatewayState::Failed && err)
      std::cerr << "error: " << err->what();
    if (state == core::GatewayState::Stopped) {
      // gateway().report(std::cerr);
      auto elapsed = tbs::MonoClock::now() - start_timestamp_;
      auto total_received = gateway().stats().total_received;
      TOOLBOX_INFO << " read " << total_received << " in "
                   << elapsed.count() / 1e9 << " s"
                   << " at " << (1e3 * total_received / elapsed.count())
                   << " mio/s";
    }
  };

  void on_tick(const core::Tick &e) {
    // TOOLBOX_INFO << e;
    auto &ins = instruments_[e.venue_instrument_id];
    if (!ins.empty()) {
      std::cout << "sym:'" << ins.venue_symbol() << "'," << e << std::endl;
    }
  }

  void on_instrument(const core::VenueInstrument &e) {
    // TOOLBOX_INFO  << e;
    instruments_.update(e);
  }

  void run(std::string_view venue, const core::Parameters &params) {
    TOOLBOX_INFO<<"MdServ::run(venue:'"<<venue<<"',params:"<<params<<")";

    using SpbProtocol = spb::SpbUdpProtocol<mcast::McastDgram>;
    using SpbMdGateway = mcast::McastMdGateway<SpbProtocol>;
    using MdGwFactory = ftu::Factory<core::IMdGateway, core::MdGateway>;

    auto factory = MdGwFactory::unique_ptr(
      ftu::IdFn{"SPB_MDBIN", [&] { return std::make_unique<SpbMdGateway>(reactor_); }});
      
    //TOOLBOX_DEBUG << "opts:\n" << params << "\n";
      
    gateway(factory(venue));

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
    tb::ReactorRunner runner(reactor_);
    tb::wait_termination_signal();
  }
  
  void on_reactor_state_changed(tb::Reactor*reactor, tb::io::State state) {
    TOOLBOX_INFO<<"Reactor state changed: "<<state;
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
    
    tb::Options parser{"mdserv [OPTIONS] URL"};
    try {
      core::MutableParameters opts;
      std::string venue;
      std::string config_file;
      bool help = false;

      parser('h', "help", tbu::Switch{help}, "print help")
        ('v', "verbose", tbu::Value{opts["verbose"], std::uint32_t{}}, "log level")
        ('O', "output_format", tbu::Value{opts["output_format"], std::string_view{}}.default_value("csv"), "output format")
        ('c', "config", tbu::Value{config_file}.required(), "config file")
        (tbu::Value{venue}.required(), "VENUE")
      .parse(argc, argv);

      if(help) {
        std::cerr << parser << std::endl;
        return EXIT_SUCCESS;
      }
      if(!config_file.empty()) {
        opts.parse_json_file(config_file);
      }

      run(venue, opts);
      return EXIT_SUCCESS;
    } catch (std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << std::endl << parser << std::endl;
      return EXIT_FAILURE;
    }
  }

private:
  tb::MonoTime start_timestamp_;
  std::unique_ptr<core::IMdGateway> gateway_;
  core::InstrumentsCache instruments_;
  io::Reactor& reactor_;
};

} // namespace ft

int main(int argc, char *argv[]) {
  ft::MdServ app(ft::io::current_reactor());
  return app.main(argc, argv);
}