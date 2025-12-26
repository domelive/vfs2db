#!/bin/bash

mkdir -p build && cd build

cmake ..

make

mkdir -p /tmp/test
./vfs2db -f -o db=../../test.db /tmp/test
