#!/bin/bash

mkdir -p build && cd build

cmake ..

make

mkdir -p /tmp/test
./src/vfs2db -f -o db=../massive_legend.db -o log=trace /tmp/test
