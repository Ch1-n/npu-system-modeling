#include <systemc>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/socket.hh"
#include "base/statistics.hh"
#include "base/trace.hh"
#include "arch/riscv/npu_bridge.hh"
#include "sc_logger.hh"
#include "sc_module.hh"
#include "sim/cxx_config_ini.hh"
#include "sim/cxx_manager.hh"
#include "sim/init_signals.hh"
#include "sim/stat_control.hh"
#include "stats.hh"

std::string filename = "stats-chapter-02.txt";

namespace {

struct AddRequest {
    std::uint64_t handle;
    std::uint64_t lhs;
    std::uint64_t rhs;
};

std::ostream& operator<<(std::ostream& os, const AddRequest& request) {
    return os << "{handle=" << request.handle << ", lhs=" << request.lhs
              << ", rhs=" << request.rhs << "}";
}

class EchoAddDevice final : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> clk{"clk"};

    SC_HAS_PROCESS(EchoAddDevice);

    explicit EchoAddDevice(sc_core::sc_module_name name)
        : sc_core::sc_module(name), requests_("requests", 1) {
        SC_THREAD(run);
    }

    std::uint64_t submit_add(std::uint64_t lhs, std::uint64_t rhs) {
        if (requests_.num_free() == 0)
            return 0;

        const std::uint64_t handle = next_handle_++;
        requests_.write(AddRequest{handle, lhs, rhs});
        std::cout << "bridge accept: handle=" << handle
                  << " time=" << sc_core::sc_time_stamp() << '\n';
        return handle;
    }

    bool take(std::uint64_t handle, std::uint64_t& value) {
        const auto it = completed_.find(handle);
        if (it == completed_.end())
            return false;
        value = it->second;
        completed_.erase(it);
        return true;
    }

    bool has_response() const { return !completed_.empty(); }
    std::uint64_t completed_count() const { return completed_count_; }
    std::uint64_t last_result() const { return last_result_; }
    sc_core::sc_event response_available;

private:
    sc_core::sc_fifo<AddRequest> requests_;
    std::unordered_map<std::uint64_t, std::uint64_t> completed_;
    std::uint64_t next_handle_ = 1;
    std::uint64_t cycle_ = 0;
    std::uint64_t completed_count_ = 0;
    std::uint64_t last_result_ = 0;

    void run() {
        while (true) {
            wait(clk.posedge_event());
            ++cycle_;

            AddRequest request{};
            if (!requests_.nb_read(request))
                continue;

            std::cout << "device issue: handle=" << request.handle
                      << " cycle=" << cycle_ << '\n';
            wait(3, sc_core::SC_NS);

            const std::uint64_t result = request.lhs + request.rhs;
            completed_.emplace(request.handle, result);
            ++completed_count_;
            last_result_ = result;
            std::cout << "device complete: handle=" << request.handle
                      << " value=" << result
                      << " time=" << sc_core::sc_time_stamp() << '\n';
            response_available.notify(sc_core::SC_ZERO_TIME);
        }
    }
};

namespace {

EchoAddDevice* active_device = nullptr;

} // namespace

std::uint64_t
npu_systemc_add_callback(std::uint64_t lhs, std::uint64_t rhs)
{
    if (active_device == nullptr)
        SC_REPORT_FATAL("npu_systemc_add", "SystemC device is not connected");

    const std::uint64_t handle = active_device->submit_add(lhs, rhs);
    if (handle == 0)
        SC_REPORT_FATAL("npu_systemc_add", "Echo/Add request was rejected");

    std::uint64_t result = 0;
    while (!active_device->take(handle, result))
        sc_core::wait(active_device->response_available);

    std::cout << "custom-0 return: handle=" << handle
              << " value=" << result
              << " time=" << sc_core::sc_time_stamp() << '\n';
    return result;
}

class Gem5Host final : public Gem5SystemC::Module {
public:
    SC_HAS_PROCESS(Gem5Host);

    Gem5Host(sc_core::sc_module_name name, const std::string& config_path,
             gem5::Tick max_ticks, EchoAddDevice& device)
        : Gem5SystemC::Module(name), max_ticks_(max_ticks), device_(device) {
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
        if (!config_file_->load(config_path.c_str()))
            SC_REPORT_FATAL("Gem5Host", "unable to load gem5 config.ini");

        config_manager_ = std::make_unique<gem5::CxxConfigManager>(
            *config_file_);
        CxxConfig::statsEnable();

        try {
            config_manager_->findAllObjects();
            for (gem5::SimObject* object : config_manager_->objectsInOrder)
                config_manager_->bindObjectPorts(object);
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

    bool passed() const { return passed_; }

private:
    Gem5SystemC::Logger logger_;
    std::unique_ptr<gem5::CxxIniFile> config_file_;
    std::unique_ptr<gem5::CxxConfigManager> config_manager_;
    gem5::Tick max_ticks_;
    EchoAddDevice& device_;
    bool passed_ = false;

    void run() {
        const gem5::GlobalSimLoopExitEvent* event = simulate(max_ticks_);
        catchup();
        const bool guest_ok = event->getCode() == 12
            && event->getCause() == "exiting with last active thread context";
        passed_ = guest_ok && device_.completed_count() == 1
            && device_.last_result() == 12;

        std::cout << "gem5 exit: tick=" << gem5::curTick()
                  << " cause=\"" << event->getCause()
                  << "\" code=" << event->getCode() << '\n';
        std::cout << "bridge result: " << device_.last_result()
                  << " (expected 12)\n";
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: " << argv[0] << " <config.ini> [max_ticks]\n";
        return EXIT_FAILURE;
    }

    gem5::Tick max_ticks = 1000000000;
    if (argc == 3) {
        const std::string text = argv[2];
        const auto result = std::from_chars(
            text.data(), text.data() + text.size(), max_ticks);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
            || max_ticks == 0) {
            std::cerr << "max_ticks must be a positive integer\n";
            return EXIT_FAILURE;
        }
    }

    sc_core::sc_set_time_resolution(1, sc_core::SC_PS);
    sc_core::sc_clock npu_clock{"npu_clock", sc_core::sc_time(1250,
                                                                sc_core::SC_PS)};
    EchoAddDevice device{"echo_add"};
    device.clk(npu_clock);
    active_device = &device;
    gem5::RiscvISAInst::setNpuAddCallback(&npu_systemc_add_callback);
    Gem5Host gem5{"gem5", argv[1], max_ticks, device};

    sc_core::sc_start();
    CxxConfig::statsDump();

    const bool passed = gem5.passed();
    std::cout << (passed ? "PASS" : "FAIL")
              << " echo_add_systemc_host\n";
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
