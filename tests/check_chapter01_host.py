#!/usr/bin/env python3
"""Run the chapter 1 smoke and failure paths with wall-clock timeouts."""

import argparse
from pathlib import Path
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("host", type=Path)
parser.add_argument("config", type=Path)
args = parser.parse_args()
host = str(args.host.resolve())
config = str(args.config.resolve())

cases = [
    ("normal_exit", [config], 0, "PASS gem5_systemc_host"),
    ("tick_limit", [config, "1"], 1, "simulate() limit reached"),
    ("invalid_limit", [config, "-1"], 1, "max_ticks must be a positive integer"),
    ("zero_limit", [config, "0"], 1, "max_ticks must be a positive integer"),
    ("usage", [], 1, "usage:"),
]
for name, arguments, expected_code, expected_text in cases:
    result = subprocess.run(
        [host, *arguments], capture_output=True, text=True, timeout=60
    )
    output = result.stdout + result.stderr
    assert result.returncode == expected_code, (name, result.returncode, output)
    assert expected_text in output, (name, output)
    if expected_code == 0:
        assert "Hello world!" in output, output
    else:
        assert "PASS gem5_systemc_host" not in output, output
    print(f"PASS {name}")
