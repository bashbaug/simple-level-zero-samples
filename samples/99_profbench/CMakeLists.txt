# Copyright (c) 2026 Ben Ashbaugh
#
# SPDX-License-Identifier: MIT

add_level_zero_sample(
    TEST
    NUMBER 99
    TARGET profbench
    SOURCES main.cpp
    KERNELS profbench_kernel64.spv)
