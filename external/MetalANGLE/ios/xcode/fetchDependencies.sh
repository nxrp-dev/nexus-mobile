#!/usr/bin/env bash
# Copyright 2018 The ANGLE Project Authors. All rights reserved.
# Copyright (c) 2026 Kevin Collins.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
angle_root="$(cd "$script_dir/../.." && pwd)"

# Nexus Mobile tracks these dependencies directly. Never replace local files
# with a fresh clone: that would discard changes owned by the parent repository.
for source_file in \
    third_party/glslang/src/CMakeLists.txt \
    third_party/spirv-cross/src/CMakeLists.txt \
    third_party/jsoncpp/BUILD.gn \
    third_party/jsoncpp/source/CMakeLists.txt; do
    if [[ ! -f "$angle_root/$source_file" ]]; then
        echo "Missing vendored dependency: $source_file; restore it from Nexus Mobile." >&2
        exit 1
    fi
done

echo 'Using the dependencies checked into Nexus Mobile.'
