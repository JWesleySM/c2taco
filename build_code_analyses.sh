#!/bin/bash

set -e

if [ $# -lt 1 ]; then
  echo "Usage $0 <path-to-llvm-build-dir>"
  exit 1
fi

LLVM_BUILD_DIR=$(realpath -L $1)
cc=${LLVM_BUILD_DIR}/bin/clang
cxx=${LLVM_BUILD_DIR}/bin/clang++

for dir in code_analysis/*; do
  if [ -f $dir ]; then
    continue
  fi
  cd $dir
  mkdir -p build && cd build
  cmake .. -DLLVM_DIR=${LLVM_BUILD_DIR}/lib/cmake/llvm -DClang_DIR=${LLVM_BUILD_DIR}/lib/cmake/clang -DCMAKE_C_COMPILER=$cc -DCMAKE_CXX_COMPILER=$cxx
  make -j$(nproc)
  cd ../../../
done