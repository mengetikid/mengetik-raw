#!/bin/bash
# Build LibRaw -> WebAssembly (wasm32-wasi reactor) dengan Zig
set -e
cd "$(dirname "$0")"
FL="-target wasm32-wasi -O2 -DLIBRAW_NOTHREADS -DNO_JPEG -DNO_JASPER -DNO_LCMS -DLIBRAW_NODLL -Istub -ILibRaw -Wno-everything -fno-rtti"
mkdir -p obj
SRCS=$(find LibRaw/src -name '*.cpp' ! -name '*_ph.cpp')
for s in $SRCS wrap.cpp; do o=obj/$(echo $s | tr '/' '_').o; if [ ! -f $o ] || [ $s -nt $o ]; then echo "$s"; fi; done | xargs -P 2 -I{} sh -c 'o=obj/$(echo {} | tr "/" "_").o; python3 -m ziglang c++ '"$FL"' -c {} -o $o'
python3 -m ziglang c++ -target wasm32-wasi -O2 -mexec-model=reactor -fno-rtti obj/*.o -o libraw-mengetik.wasm -Wl,--export=malloc -Wl,--export=free -Wl,--strip-all -Wl,--initial-memory=33554432 -Wl,--max-memory=4294901760
ls -la libraw-mengetik.wasm
