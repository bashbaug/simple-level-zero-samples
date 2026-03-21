# Copyright (c) 2026 Ben Ashbaugh
#
# SPDX-License-Identifier: MIT

add_level_zero_sample(
    TEST
    NUMBER 99
    TARGET sysmemtester
    SOURCES main.cpp
    KERNELS sysmemtester_kernel64.spv)
