#!/bin/bash

set -e

if [ $# -lt 1 ]; then
  echo "Usage $0 <path-to-llvm-build-dir>"
  exit 1
fi

LLVM_BUILD_DIR=$1

for dir in code_analysis/*; do
  if [ -f $dir ]; then
    continue
  fi
  cd $dir
  pwd
  mkdir -p build && cd build
  cmake .. -DLLVM_DIR=${LLVM_BUILD_DIR}/lib/cmake/llvm -DClang_DIR=${LLVM_BUILD_DIR}/lib/cmake/clang
  make -j$(nproc)
  cd ../../../
done