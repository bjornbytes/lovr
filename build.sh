#!/bin/sh

rm -rf build
mkdir build
cd build
emcmake cmake .. && emmake make
mkdir game
echo "function lovr.load()\n  print('hi')\nend" > game/main.lua
../package.sh game
