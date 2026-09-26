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
compiler = os.environ.get("CXX", "c++")
COMMON = [compiler] + (["c++"] if Path(compiler).stem == "zig" else [])
COMMON += ["-std=c++20", "-Wall", "-Wextra", "-Werror",
          "-include", "test/dashboard/stubs/HostTime.h", "-Itest/dashboard/stubs", "-Isrc", "-Isrc/dashboard",
          "-Ilib/Memory", "-Ilib/I18n", "-I.pio/libdeps/dashboard/ArduinoJson/src"]
if os.name != "nt":
    COMMON += ["-fsanitize=address,undefined"]
else:
    COMMON += ["-D_CRT_SECURE_NO_WARNINGS"]
MODEL = "src/dashboard/DashboardModel.cpp"
STORE = "src/dashboard/DashboardStore.cpp"
PROJECTS = "src/dashboard/DashboardProjects.cpp"
EMOJI = "src/dashboard/DashboardEmoji.cpp"
THAI = "src/dashboard/DashboardThai.cpp"
SUITES = {
    "model": [MODEL],
    "store": [MODEL, PROJECTS, STORE],
    "projects": [MODEL, PROJECTS],
    "client": [MODEL, PROJECTS, STORE, "src/dashboard/TodoistClient.cpp"],
    "setup": [MODEL, PROJECTS, STORE, "src/dashboard/TodoistClient.cpp", "src/activities/dashboard/DashboardSetupActivity.cpp"],
    "view": [MODEL, EMOJI, THAI, "src/components/themes/DashboardTheme.cpp"],
    "emoji": [EMOJI, THAI],
    "thai": [EMOJI, THAI],
}
for name, sources in SUITES.items():
    binary = BUILD / (name + (".exe" if os.name == "nt" else ""))
    subprocess.run(COMMON + sources + [f"test/dashboard/{name}_test.cpp", "-o", str(binary)], check=True)
    with tempfile.TemporaryDirectory(prefix="x3-dashboard-") as directory:
        args = [] if name in ("model", "emoji", "thai") else [str(ROOT / "build/dashboard-preview") if name == "view" else directory]
        subprocess.run([str(binary), *args], check=True)
