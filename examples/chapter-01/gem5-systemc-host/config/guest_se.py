import argparse

import m5
from m5.objects import AddrRange
from m5.objects import DDR3_1600_8x8
from m5.objects import MemCtrl
from m5.objects import Process
from m5.objects import RiscvMinorCPU
from m5.objects import Root
from m5.objects import SEWorkload
from m5.objects import SrcClockDomain
from m5.objects import System
from m5.objects import SystemXBar
from m5.objects import VoltageDomain


parser = argparse.ArgumentParser()
parser.add_argument("--binary", required=True)
parser.add_argument("--generate-only", action="store_true")
args = parser.parse_args()

system = System()
system.clk_domain = SrcClockDomain(
    clock="1.6GHz", voltage_domain=VoltageDomain()
)
system.mem_mode = "timing"
system.cache_line_size = 64
system.mem_ranges = [AddrRange("512MiB")]

system.cpu = RiscvMinorCPU()
system.membus = SystemXBar()
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports
system.cpu.createInterruptController()

system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports
system.system_port = system.membus.cpu_side_ports

system.workload = SEWorkload.init_compatible(args.binary)
process = Process(cmd=[args.binary])
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system=False, system=system)
m5.instantiate()

if not args.generate_only:
    exit_event = m5.simulate()
    print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
