/*
// Copyright (c) 2022 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include <popl/popl.hpp>

#include "ze_api.h"

#include "bmp.hpp"
#include "util.hpp"

#include <chrono>

const char* bmp_filename = "julia.bmp";
const char* spv_filename = "julia.spv";

const float cr = -0.123f;
const float ci =  0.745f;

#define CHECK_CALL( _call )                                                 \
    do {                                                                    \
        ze_result_t result = _call;                                         \
        if (result != ZE_RESULT_SUCCESS) {                                  \
            printf("%s returned %u!\n", #_call, result);                    \
        }                                                                   \
    } while (0)

int main(
    int argc,
    char** argv )
{
    int platformIndex = 0;
    int deviceIndex = 0;

    std::string buildOptions;

    uint32_t iterations = 16;
    uint32_t gwx = 512;
    uint32_t gwy = 512;
    uint32_t lwx = 0;
    uint32_t lwy = 0;

    {
        popl::OptionParser op("Supported Options");
        op.add<popl::Value<int>>("p", "platform", "Platform Index", platformIndex, &platformIndex);
        op.add<popl::Value<int>>("d", "device", "Device Index", deviceIndex, &deviceIndex);
        op.add<popl::Value<std::string>>("", "options", "Program Build Options", buildOptions, &buildOptions);
        op.add<popl::Value<uint32_t>>("i", "iterations", "Iterations", iterations, &iterations);
        op.add<popl::Value<uint32_t>>("", "gwx", "Global Work Size X AKA Image Width", gwx, &gwx);
        op.add<popl::Value<uint32_t>>("", "gwy", "Global Work Size Y AKA Image Height", gwy, &gwy);
        op.add<popl::Value<uint32_t>>("", "lwx", "Local Work Size X", lwx, &lwx);
        op.add<popl::Value<uint32_t>>("", "lwy", "Local Work Size Y", lwy, &lwy);

        bool printUsage = false;
        try {
            op.parse(argc, argv);
        } catch (std::exception& e) {
            fprintf(stderr, "Error: %s\n\n", e.what());
            printUsage = true;
        }

        if (printUsage || !op.unknown_options().empty() || !op.non_option_args().empty()) {
            fprintf(stderr,
                "Usage: julia [options]\n"
                "%s", op.help().c_str());
            return -1;
        }
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

    ze_command_list_handle_t queue = nullptr;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    CHECK_CALL( zeCommandListCreateImmediate(context, devices[deviceIndex], &queueDesc, &queue) );

    printf("Reading SPIR-V from file: %s\n", spv_filename);
    std::vector<uint8_t> spirv = readSPIRVFromFile(spv_filename);

    ze_module_handle_t module = nullptr;
    ze_module_build_log_handle_t buildLog = nullptr;
    ze_module_desc_t moduleDesc = {};
    moduleDesc.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.inputSize = spirv.size();
    moduleDesc.pInputModule = spirv.data();
    moduleDesc.pBuildFlags = buildOptions.c_str();
    CHECK_CALL( zeModuleCreate(context, devices[deviceIndex], &moduleDesc, &module, &buildLog) );

    ze_kernel_handle_t kernel = nullptr;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
    kernelDesc.pKernelName = "Julia";
    CHECK_CALL( zeKernelCreate(module, &kernelDesc, &kernel) );

    uint8_t* pDst = nullptr;
    ze_host_mem_alloc_desc_t hostAllocDesc = {};
    hostAllocDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    CHECK_CALL( zeMemAllocHost(context, &hostAllocDesc, gwx * gwy * sizeof(uint32_t), 0, (void**)&pDst) );

    // execution
    {
        CHECK_CALL( zeKernelSetArgumentValue(kernel, 0, sizeof(pDst), &pDst) );
        CHECK_CALL( zeKernelSetArgumentValue(kernel, 1, sizeof(cr), &cr) );
        CHECK_CALL( zeKernelSetArgumentValue(kernel, 2, sizeof(ci), &ci) );

        printf("Executing the kernel %u times\n", iterations);
        printf("Global Work Size = ( %u, %u )\n", gwx, gwy);
        ze_group_count_t groupCount = {};
        if( lwx > 0 && lwy > 0 )
        {
            printf("Local Work Size = ( %u, %u )\n", lwx, lwy);
        }
        else
        {
            uint32_t lwz;
            CHECK_CALL( zeKernelSuggestGroupSize(kernel, gwx, gwy, 1, &lwx, &lwy, &lwz) );
            printf("Local work size = NULL --> ( %u, %u )\n", lwx, lwy);
        }

        CHECK_CALL( zeKernelSetGroupSize(kernel, lwx, lwy, 1) );

        groupCount.groupCountX = gwx / lwx;
        groupCount.groupCountY = gwy / lwy;
        groupCount.groupCountZ = 1;

        auto start = std::chrono::system_clock::now();
        for( uint32_t i = 0; i < iterations; i++ )
        {
            CHECK_CALL( zeCommandListAppendLaunchKernel(queue, kernel, &groupCount, nullptr, 0, nullptr) );
        }

        // Ensure all processing is complete before stopping the timer.
        CHECK_CALL( zeCommandListAppendBarrier(queue, event, 0, nullptr) );
        CHECK_CALL( zeEventHostSynchronize(event, UINT64_MAX) );
        CHECK_CALL( zeEventHostReset(event) );

        auto end = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsed_seconds = end - start;
        printf("Finished in %f seconds\n", elapsed_seconds.count());
    }

    // save bitmap
    {
        BMP::save_image(pDst, gwx, gwy, bmp_filename);
        printf("Wrote image file %s\n", bmp_filename);
    }

    // Note: should really clean up resources here!

    return 0;
}
