#!/bin/bash
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build build
./build/excavation-sim
