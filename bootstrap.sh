#!/bin/sh

# WARNING: This file is meant to be a minimal starting point for CMake and LSP newcomers.
#          It is strongly advised to keep the current feature set and don't add more.
#          Instead, have a look at the CMake manual to customize your build experience.

set -eu
BUILD_DIR="${1:-build}"
cmake -S "." -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=YES

ln -s "${BUILD_DIR}/compile_commands.json" . 2>/dev/null || echo "[WARN] Reusing existing compile_commands.json."

echo "compile_commands.json setup. Happy hacking with your LSP!"
echo "Rerun this file if you change the CMakeLists.txt!"
