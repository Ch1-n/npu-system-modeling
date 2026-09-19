#!/usr/bin/env python3
"""Check the chapter 2 Echo/Add bridge smoke path."""

import argparse
from pathlib import Path
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("host", type=Path)
parser.add_argument("config", type=Path)
args = parser.parse_args()

result = subprocess.run(
    [str(args.host.resolve()), str(args.config.resolve())],
    capture_output=True,
    text=True,
    timeout=60,
)
output = result.stdout + result.stderr

assert result.returncode == 0, output
for expected in (
    "bridge accept: handle=1",
    "device issue: handle=1",
    "device complete: handle=1 value=12",
    "bridge result: 12 (expected 12)",
    "PASS echo_add_systemc_host",
):
    assert expected in output, (expected, output)

print("PASS chapter02_echo_add")
