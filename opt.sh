#!/bin/bash
/Users/hydai/Program/github/LLVM-keynote/sample/llvm-z80/build/bin/opt -basicaa -tailcallelim -gvn -dse -stats ./code.ll -S -o ./code-opt.ll
