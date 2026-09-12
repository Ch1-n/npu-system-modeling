#include <systemc>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "base/socket.hh"
#include "base/statistics.hh"
#include "base/trace.hh"
#include "sc_logger.hh"
#include "sc_module.hh"
#include "sim/cxx_config_ini.hh"
#include "sim/cxx_manager.hh"
#include "sim/init_signals.hh"
#include "sim/stat_control.hh"
#include "stats.hh"

std::string filename = "stats-chapter-01.txt";

namespace {

class Gem5Host final : public Gem5SystemC::Module {
public:
    SC_HAS_PROCESS(Gem5Host);

    Gem5Host(sc_core::sc_module_name name, const std::string& config_path)
        : Gem5SystemC::Module(name) {
        SC_THREAD(run);

        gem5::trace::setDebugLogger(&logger_);
        Gem5SystemC::setTickFrequency();
        Gem5SystemC::Module::setupEventQueues(*this);

        if (sc_core::sc_get_time_resolution()
            != sc_core::sc_time(1, sc_core::SC_PS)) {
            SC_REPORT_FATAL("Gem5Host", "SystemC resolution must be 1 ps");
        }

        gem5::initSignals();
        gem5::statistics::initSimStats();
        gem5::statistics::registerHandlers(CxxConfig::statsReset,
                                            CxxConfig::statsDump);
        gem5::trace::enable();
        gem5::ListenSocket::disableAll();

        config_file_ = std::make_unique<gem5::CxxIniFile>();
        if (!config_file_->load(config_path.c_str())) {
            SC_REPORT_FATAL("Gem5Host", "unable to load gem5 config.ini");
        }

        config_manager_ = std::make_unique<gem5::CxxConfigManager>(
            *config_file_);
        CxxConfig::statsEnable();

        try {
            config_manager_->findAllObjects();
            for (gem5::SimObject* object : config_manager_->objectsInOrder) {
                config_manager_->bindObjectPorts(object);
            }
            config_manager_->instantiate(false);
        } catch (const gem5::CxxConfigManager::Exception& error) {
            const std::string message = error.name + ": " + error.message;
            SC_REPORT_FATAL("Gem5Host", message.c_str());
        }
    }

    void end_of_elaboration() override {
        try {
            config_manager_->initState();
            config_manager_->startup();
        } catch (const gem5::CxxConfigManager::Exception& error) {
            const std::string message = error.name + ": " + error.message;
            SC_REPORT_FATAL("Gem5Host", message.c_str());
        }
    }

    int exit_code() const { return exit_code_; }

private:
    Gem5SystemC::Logger logger_;
    std::unique_ptr<gem5::CxxIniFile> config_file_;
    std::unique_ptr<gem5::CxxConfigManager> config_manager_;
    int exit_code_ = -1;

    void run() {
        const gem5::GlobalSimLoopExitEvent* event = simulate();
        exit_code_ = event->getCode();
        std::cout << "gem5 exit: tick=" << gem5::curTick()
                  << " cause=\"" << event->getCause() << "\""
                  << " code=" << exit_code_ << '\n';
        sc_core::sc_stop();
    }
};

class Heartbeat final : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> clk{"clk"};

    SC_HAS_PROCESS(Heartbeat);

    explicit Heartbeat(sc_core::sc_module_name name)
        : sc_core::sc_module(name) {
        SC_METHOD(on_tick);
        sensitive << clk.pos();
        dont_initialize();
    }

    std::uint64_t cycles() const { return cycles_; }

private:
    std::uint64_t cycles_ = 0;

    void on_tick() { ++cycles_; }
};

} // namespace

int sc_main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <config.ini>\n";
        return EXIT_FAILURE;
    }

    sc_core::sc_set_time_resolution(1, sc_core::SC_PS);
    sc_core::sc_clock npu_clock{
        "npu_clock", sc_core::sc_time(1250, sc_core::SC_PS)};
    Heartbeat heartbeat{"heartbeat"};
    heartbeat.clk(npu_clock);
    Gem5Host gem5{"gem5", argv[1]};

    sc_core::sc_start();
    CxxConfig::statsDump();

    std::cout << "SystemC stop: time=" << sc_core::sc_time_stamp()
              << " npu_cycles=" << heartbeat.cycles() << '\n';
    return gem5.exit_code() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
