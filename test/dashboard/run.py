#!/usr/bin/env python3
"""Build and run dashboard host tests; no hardware or credentials required."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
os.chdir(ROOT)
BUILD = ROOT / "build/dashboard-tests"
BUILD.mkdir(parents=True, exist_ok=True)
COMMON = [os.environ.get("CXX", "c++"), "-std=c++20", "-Wall", "-Wextra", "-Werror",
          "-fsanitize=address,undefined", "-Itest/dashboard/stubs", "-Isrc", "-Isrc/dashboard",
          "-Ilib/Memory", "-Ilib/I18n", "-I.pio/libdeps/dashboard/ArduinoJson/src"]
MODEL = "src/dashboard/DashboardModel.cpp"
STORE = "src/dashboard/DashboardStore.cpp"
SUITES = {
    "model": [MODEL],
    "store": [MODEL, STORE],
    "client": [MODEL, STORE, "src/dashboard/TodoistClient.cpp"],
    "view": [MODEL, "src/components/themes/DashboardTheme.cpp"],
}
for name, sources in SUITES.items():
    binary = BUILD / name
    subprocess.run(COMMON + sources + [f"test/dashboard/{name}_test.cpp", "-o", str(binary)], check=True)
    with tempfile.TemporaryDirectory(prefix="x3-dashboard-") as directory:
        args = [] if name == "model" else [str(ROOT / "build/dashboard-preview") if name == "view" else directory]
        subprocess.run([str(binary), *args], check=True)
