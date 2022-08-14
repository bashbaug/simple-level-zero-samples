# Copyright (c) 2021-2022 Ben Ashbaugh
#
# SPDX-License-Identifier: MIT

add_level_zero_sample(
    NUMBER 00
    TARGET juliavk
    CATEGORY vulkan
    SOURCES main.cpp
    KERNELS juliavk.vert.spv juliavk.frag.spv juliavk.spv
    INCLUDES ${Vulkan_INCLUDE_DIR}
    LIBS ${Vulkan_LIBRARY} glfw)
