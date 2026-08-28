"""Make the `native_test` PlatformIO environment find a gcc-compatible host
compiler without requiring one to be separately installed.

The native platform's own builder (platformio/platforms/native/builder/main.py)
deletes any CC/CXX we set here and re-detects "gcc"/"g++" via PATH, so
setting env['CC'] directly from this pre-script has no effect. Instead this
prepends test/native_tools/ (gcc.cmd/g++.cmd/ar.cmd/ranlib.cmd wrapping the
zig cc/c++/ar/ranlib drivers bundled in .venv via the `ziglang` pip package)
to the PATH the build sees, so that detection finds our wrappers.
"""

Import("env")

import os

native_tools = os.path.join(env["PROJECT_DIR"], "test", "native_tools")
env["ENV"]["PATH"] = native_tools + os.pathsep + env["ENV"]["PATH"]
