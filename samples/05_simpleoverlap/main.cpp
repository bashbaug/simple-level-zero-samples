/*
// Copyright (c) 2022 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include <popl/popl.hpp>

#include "ze_api.h"

#include "util.hpp"

#include <chrono>

using test_clock = std::chrono::high_resolution_clock;

constexpr int maxKernels = 64;
constexpr int testIterations = 32;

int numKernels = 8;
int numIterations = 1;
uint32_t numElements = 1;

const char* spv_filename = "timesink.spv";

int main(
    int argc,
    char** argv )
{
    int platformIndex = 0;
    int deviceIndex = 0;

    {
        popl::OptionParser op("Supported Options");
        op.add<popl::Value<int>>("p", "platform", "Platform Index", platformIndex, &platformIndex);
        op.add<popl::Value<int>>("d", "device", "Device Index", deviceIndex, &deviceIndex);
        op.add<popl::Value<int>>("k", "kernels", "Kernel to Execute", numKernels, &numKernels);
        op.add<popl::Value<int>>("i", "iterations", "Kernel Iterations", numIterations, &numIterations);
        op.add<popl::Value<uint32_t>>("e", "elements", "Number of ND-Range Elements", numElements, &numElements);

        bool printUsage = false;
        try {
            op.parse(argc, argv);
        } catch (std::exception& e) {
            fprintf(stderr, "Error: %s\n\n", e.what());
            printUsage = true;
        }

        if (printUsage || !op.unknown_options().empty() || !op.non_option_args().empty()) {
            fprintf(stderr,
                "Usage: simpleoverlap [options]\n"
                "%s", op.help().c_str());
            return -1;
        }
    }

    if (numKernels > maxKernels) {
        printf("Number of kernels is %d, which exceeds the maximum of %d.\n", numKernels, maxKernels);
        printf("The number of kernels will be set to %d instead.\n", maxKernels);
        numKernels = maxKernels;
    }

    CHECK_CALL( zeInit(0) );

    uint32_t driverCount = 0;
    CHECK_CALL( zeDriverGet(&driverCount, nullptr) );
    printf("Enumerated %u drivers.\n\n", driverCount);

    std::vector<ze_driver_handle_t> drivers(driverCount);
    CHECK_CALL( zeDriverGet(&driverCount, drivers.data()) );

    uint32_t deviceCount = 0;
    CHECK_CALL( zeDeviceGet(drivers[platformIndex], &deviceCount, nullptr) );

    std::vector<ze_device_handle_t> devices(deviceCount);
    CHECK_CALL( zeDeviceGet(drivers[platformIndex], &deviceCount, devices.data()) );

    ze_device_properties_t deviceProps = {};
    deviceProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    CHECK_CALL( zeDeviceGetProperties(devices[deviceIndex], &deviceProps) );

    printf("Running on device: %s\n", deviceProps.name);

    ze_context_handle_t context = nullptr;
    ze_context_desc_t contextDesc = {};
    contextDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    CHECK_CALL( zeContextCreate(drivers[platformIndex], &contextDesc, &context) );

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;
    CHECK_CALL( zeEventPoolCreate(context, &eventPoolDesc, 0, nullptr, &eventPool) );

    ze_event_handle_t event = nullptr;
    ze_event_desc_t eventDesc = {};
    eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    CHECK_CALL( zeEventCreate(eventPool, &eventDesc, &event) );

    printf("Reading SPIR-V from file: %s\n", spv_filename);
    std::vector<uint8_t> spirv = readSPIRVFromFile(spv_filename);

    ze_module_handle_t module = nullptr;
    ze_module_build_log_handle_t buildLog = nullptr;
    ze_module_desc_t moduleDesc = {};
    moduleDesc.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.inputSize = spirv.size();
    moduleDesc.pInputModule = spirv.data();
    CHECK_CALL( zeModuleCreate(context, devices[deviceIndex], &moduleDesc, &module, &buildLog) );

    ze_kernel_handle_t kernel = nullptr;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
    kernelDesc.pKernelName = "TimeSink";
    CHECK_CALL( zeKernelCreate(module, &kernelDesc, &kernel) );

    void* dptr = nullptr;
    ze_host_mem_alloc_desc_t hostAllocDesc = {};
    hostAllocDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    CHECK_CALL( zeMemAllocHost(context, &hostAllocDesc, numElements * sizeof(float), 0, &dptr) );

    CHECK_CALL( zeKernelSetArgumentValue(kernel, 0, sizeof(dptr), &dptr) );
    CHECK_CALL( zeKernelSetArgumentValue(kernel, 1, sizeof(numIterations), &numIterations) );

    // execution
    {
        printf("Running %d iterations of %d kernels...\n", testIterations, numKernels);

        ze_command_list_handle_t queue = nullptr;
        ze_command_queue_desc_t queueDesc = {};
        queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        queueDesc.ordinal = 0;
        queueDesc.index = 0;
        CHECK_CALL( zeCommandListCreateImmediate(context, devices[deviceIndex], &queueDesc, &queue) );

        uint32_t lwx, lwy, lwz;
        CHECK_CALL( zeKernelSuggestGroupSize(kernel, numElements, 1, 1, &lwx, &lwy, &lwz) );
        CHECK_CALL( zeKernelSetGroupSize(kernel, lwx, 1, 1) );

        ze_group_count_t groupCount = {};
        groupCount.groupCountX = numElements / lwx;
        groupCount.groupCountY = 1;
        groupCount.groupCountZ = 1;

        float best = 999.0f;
        for (int test = 0; test < testIterations; test++) {
            auto start = test_clock::now();
            for (int i = 0; i < numKernels; i++) {
                CHECK_CALL( zeCommandListAppendLaunchKernel(queue, kernel, &groupCount, nullptr, 0, nullptr) );
            }
            CHECK_CALL( zeCommandListAppendBarrier(queue, event, 0, nullptr) );
            CHECK_CALL( zeEventHostSynchronize(event, UINT64_MAX) );
            CHECK_CALL( zeEventHostReset(event) );

            auto end = test_clock::now();
            std::chrono::duration<float> elapsed_seconds = end - start;
            best = std::min(best, elapsed_seconds.count());
        }

        printf("Best time was %f seconds.\n", best);
    }

    printf("Cleaning up...\n");

    // Note: should really clean up resources here!

    return 0;
}
