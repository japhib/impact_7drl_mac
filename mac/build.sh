#!/bin/bash

# fail fast
set -e

mkdir out
pushd out

# I think you probably need XCode system tools installed for this
# something like:
#     xcode-select --install
cmake ../.. -G "Unix Makefiles" -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++

make -j 6
cp ./impact_7drl ../
