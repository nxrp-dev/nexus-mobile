#!/bin/bash

#for file in spirv_*.{cpp,hpp} include/spirv_cross/*.{hpp,h} ../../../../../../nexus-lab/examples/graphics/spirv/cpp/*.cpp main.cpp
for file in spirv_*.{cpp,hpp} main.cpp
do
    echo "Formatting file: $file ..."
    clang-format -style=file -i $file
done
