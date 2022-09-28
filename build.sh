#!/bin/bash

mkdir build
pushd build &> /dev/null
cmake ..
make -j4
popd &> /dev/null
