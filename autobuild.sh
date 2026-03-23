#!/bin/bash

# 确保脚本发生错误时立刻退出
set -e

echo "[1/4] Preparing directories..."
mkdir -p build
mkdir -p bin
rm -rf build/*

echo "[2/4] Initializing CMake..."
cd build && cmake ..

echo "[3/4] Compiling source code (Make)..."
make -j4

echo "[4/4] Build complete! Executables are in the bin/ directory."
