#!/bin/bash
#
# Copyright (C) [2021] Futurewei Technologies, Inc. All rights reserved.
#
# OpenArkCompiler is licensed underthe Mulan Permissive Software License v2.
# You can use this software according to the terms and conditions of the MulanPSL - 2.0.
# You may obtain a copy of MulanPSL - 2.0 at:
#
#   https://opensource.org/licenses/MulanPSL-2.0
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
# FIT FOR A PARTICULAR PURPOSE.
# See the MulanPSL - 2.0 for more details.
#
#!/bin/bash -x

BIN=$MAPLE_COMPILER_ROOT/bin
AST2MPL=$BIN/ast2mpl
MPLCG=$BIN/ark-clang-release/mplcg
MPLSH=$MAPLE_RUNTIME_ROOT/bin/x86_64/mplsh
GCC=gcc

script="$(basename -- $0)"
if [ $# -lt 1 ]; then
    echo "Usage: $script <C source file>"
    exit 1
fi
file="$(basename -- $1)"
file="${file%.*}"

$AST2MPL $file.c -isystem /usr/lib/gcc/x86_64-linux-gnu/5/include/
$MPLCG --no-gen-groot-list --quiet --no-pie --verbose-asm --gen-c-macro-def --no-nativeopt --maplelinker --fpic -O2 $file.mpl
sed -i -e 's/\.short /\.word /g' -e '/\.hidden\tmain$/d' -e 's/.*__mpl_personality_v0.*//' $file.s
$GCC -g3 -pie -O2 -x assembler-with-cpp -c $file.s -o $file.o
$GCC -g3 -pie -O2 -fPIC -shared -o $file.so $file.o -rdynamic

export LD_LIBRARY_PATH=$MAPLE_RUNTIME_ROOT/lib/x86_64:$LD_LIBRARY_PATH
$MPLSH $file.so
