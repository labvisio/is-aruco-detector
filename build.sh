#!/bin/bash

set -e

conan build . \
    --build=missing \
    --output-folder=./ \
    -s build_type=Release \
    -s compiler.libcxx=libstdc++11 \
    -o is-aruco-detector/*:shared=True