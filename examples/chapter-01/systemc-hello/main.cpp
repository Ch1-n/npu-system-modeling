#include <systemc>

#include <cstdint>
#include <iostream>

class ClockObserver final : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> clk{"clk"};

    SC_HAS_PROCESS(ClockObserver);

    explicit ClockObserver(sc_core::sc_module_name name)
        : sc_core::sc_module(name) {
        SC_METHOD(on_rising_edge);
        sensitive << clk.pos();
        dont_initialize();
    }

    std::uint64_t cycles() const { return cycles_; }

private:
    std::uint64_t cycles_ = 0;

    void on_rising_edge() {
        ++cycles_;
        std::cout << "cycle=" << cycles_
                  << " time=" << sc_core::sc_time_stamp() << '\n';
        if (cycles_ == 8) {
            sc_core::sc_stop();
        }
    }
};

int sc_main(int, char**) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_PS);

    sc_core::sc_clock clock{"clock", sc_core::sc_time(1, sc_core::SC_NS)};
    ClockObserver observer{"observer"};
    observer.clk(clock);

    sc_core::sc_start();

    if (observer.cycles() != 8) {
        std::cerr << "unexpected cycle count: " << observer.cycles() << '\n';
        return 1;
    }

    std::cout << "PASS systemc_hello at " << sc_core::sc_time_stamp() << '\n';
    return 0;
}
