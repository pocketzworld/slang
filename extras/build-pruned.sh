#!/usr/bin/env bash
# Builds the pruned, Release slang-compiler shared library (core module embedded) and slangc,
# with nothing else: no slang-llvm, slang-glslang, slang-rhi/gfx, slangd, slangi, slang-rt,
# tests, examples or record/replay. The same script and options on every machine.
#
#   extras/build-pruned.sh [build-dir]        (default: build-pruned)
#
# Windows: Git Bash, CMake >= 3.26 and Visual Studio 2022+ C++ tools; run it from a developer
#          shell (cl on PATH) to build with Ninja, or from a plain shell for the VS generator.
# macOS:   Xcode command-line tools and CMake (Ninja used when on PATH).
# Output:  <build-dir>/Release/bin/slang-compiler.dll (Windows) or
#          <build-dir>/Release/lib/libslang-compiler.dylib (macOS), and slangc beside.
set -euo pipefail
cd "$(dirname "$0")/.."
build=${1:-build-pruned}

git submodule update --init --depth 1 \
    external/unordered_dense external/miniz external/lz4 external/cmark external/vulkan \
    external/fast_float external/spirv-headers external/lua

generator=()
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        if command -v cl >/dev/null && command -v ninja >/dev/null; then
            generator=(-G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl)
        fi ;;
    *)
        if command -v ninja >/dev/null; then generator=(-G Ninja); fi ;;
esac

start=$(date +%s)
cmake -S . -B "$build" ${generator[@]+"${generator[@]}"} -DCMAKE_BUILD_TYPE=Release \
    -DSLANG_ENABLE_GFX=OFF -DSLANG_ENABLE_SLANGD=OFF -DSLANG_ENABLE_SLANGI=OFF \
    -DSLANG_ENABLE_SLANGRT=OFF -DSLANG_ENABLE_SLANG_GLSLANG=OFF -DSLANG_ENABLE_SLANG_PROXY=OFF \
    -DSLANG_ENABLE_TESTS=OFF -DSLANG_ENABLE_EXAMPLES=OFF -DSLANG_ENABLE_REPLAYER=OFF \
    -DSLANG_ENABLE_RECORD_REPLAY=OFF -DSLANG_ENABLE_SLANG_RHI=OFF -DSLANG_SLANG_LLVM_FLAVOR=DISABLE \
    -DSLANG_ENABLE_DXIL=OFF -DSLANG_ENABLE_CUDA=OFF -DSLANG_ENABLE_OPTIX=OFF -DSLANG_ENABLE_NVAPI=OFF \
    -DSLANG_ENABLE_AFTERMATH=OFF -DSLANG_EXCLUDE_TINT=ON -DSLANG_INSTALL_USER_SKILLS=OFF \
    -DSLANG_STANDARD_MODULE_DEVELOP_BUILD=OFF -DSLANG_ENABLE_RELEASE_DEBUG_INFO=OFF \
    -DSLANG_EMBED_CORE_MODULE=ON -DSLANG_EMBED_CORE_MODULE_SOURCE=OFF -DSLANG_ENABLE_MIMALLOC=OFF \
    -DSLANG_ENABLE_RELEASE_LTO="${SLANG_PRUNED_LTO:-OFF}"
cmake --build "$build" --config Release --target slangc --parallel

lib=$(ls "$build"/Release/bin/slang-compiler.dll "$build"/Release/lib/libslang-compiler.dylib \
      "$build"/Release/lib/libslang-compiler.so 2>/dev/null | head -1)
case "$lib" in *.dylib|*.so) strip -x "$lib" ;; esac
echo "built in $(( $(date +%s) - start )) s: $lib ($(wc -c < "$lib") bytes)"
