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
./src/vfs2db -f -o db=../databases/stress_test.db -o log=trace -o cache_enabled=1 /tmp/test 
