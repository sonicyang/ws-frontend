#!/bin/bash
LLVM_BIN=~/llvm/build/bin

clang++ \
    -D__STDC_CONSTANT_MACROS \
    -D__STDC_LIMIT_MACROS \
    -std=c++11 \
    main.cpp  \
    `${LLVM_BIN}/llvm-config --cxxflags --ldflags --libs --system-libs` \
    -O3 \
    -g3 \
    -o wsjit
