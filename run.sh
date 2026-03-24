#!/bin/bash

mkdir -p build && cd build

cmake ..

make

# Check if errors occured during compilation
if [ $? -ne 0 ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi

mkdir -p /tmp/test
./src/vfs2db -s -f -o db=../test_seq.db -o log=trace -o foreign_keys=1 -o auto_cache /tmp/test
