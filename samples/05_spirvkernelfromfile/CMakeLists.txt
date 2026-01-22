# Copyright (c) 2025 Ben Ashbaugh
#
# SPDX-License-Identifier: MIT

add_level_zero_sample(
    TEST
    NUMBER 05
    TARGET spirvkernelfromfile
    SOURCES main.cpp
    KERNELS sample_kernel64.spv)
