/*
// Copyright (c) 2019-2025 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include <popl/popl.hpp>

#include "ze_api.h"

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

//#define USE_DEVICE_MEM

#define CHECK_CALL( _call )                                                 \
    do {                                                                    \
        ze_result_t result = _call;                                         \
        if (result != ZE_RESULT_SUCCESS) {                                  \
            printf("%s returned %u!\n", #_call, result);                    \
        }                                                                   \
    } while (0)

static inline std::vector<uint8_t> readSPIRVFromFile(
    const std::string& filename )
{
    std::ifstream is(filename, std::ios::binary);
    std::vector<uint8_t> ret;

    if (!is.good()) {
        printf("Couldn't open file '%s'!\n", filename.c_str());
        return ret;
    }

    size_t filesize = 0;
    is.seekg(0, std::ios::end);
    filesize = (size_t)is.tellg();
    is.seekg(0, std::ios::beg);

    ret.reserve(filesize);
    ret.insert(
        ret.begin(),
        std::istreambuf_iterator<char>(is),
        std::istreambuf_iterator<char>() );

    return ret;
}

int main(int argc, char** argv)
{
    int platformIndex = 0;
    int deviceIndex = 0;

    std::string fileName("profbench_kernel64.spv");
    size_t numEvents = 64 * 1024;

    {
        popl::OptionParser op("Supported Options");
        op.add<popl::Value<int>>("p", "platform", "Platform Index", platformIndex, &platformIndex);
        op.add<popl::Value<int>>("d", "device", "Device Index", deviceIndex, &deviceIndex);
        op.add<popl::Value<std::string>>("", "file", "Kernel File Name", fileName, &fileName);
        op.add<popl::Value<size_t>>("n", "numevents", "Number of Events", numEvents, &numEvents);
        bool printUsage = false;
        try {
            op.parse(argc, argv);
        } catch (std::exception& e) {
            fprintf(stderr, "Error: %s\n\n", e.what());
            printUsage = true;
        }

        if (printUsage || !op.unknown_options().empty() || !op.non_option_args().empty()) {
            fprintf(stderr,
                "Usage: profbench [options]\n"
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

    ze_driver_handle_t hDriver = drivers[platformIndex];

    uint32_t deviceCount = 0;
    CHECK_CALL( zeDeviceGet(hDriver, &deviceCount, nullptr) );

    std::vector<ze_device_handle_t> devices(deviceCount);
    CHECK_CALL( zeDeviceGet(hDriver, &deviceCount, devices.data()) );

    ze_device_handle_t hDevice = devices[deviceIndex];

    ze_device_properties_t deviceProps = {};
    deviceProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    CHECK_CALL( zeDeviceGetProperties(hDevice, &deviceProps) );

    printf("Running on device: %s\n", deviceProps.name);

    // context

    ze_context_handle_t context = nullptr;
    ze_context_desc_t contextDesc = {};
    contextDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    CHECK_CALL( zeContextCreate(drivers[platformIndex], &contextDesc, &context) );

    // queue

    ze_command_list_handle_t queue = nullptr;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    CHECK_CALL( zeCommandListCreateImmediate(context, hDevice, &queueDesc, &queue) );

    // event pool

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.count = static_cast<uint32_t>(numEvents);
    CHECK_CALL( zeEventPoolCreate(context, &eventPoolDesc, 0, nullptr, &eventPool) );

    // module

    printf("Reading SPIR-V from file: %s\n", fileName.c_str());
    std::vector<uint8_t> spirv = readSPIRVFromFile(fileName);

    ze_module_handle_t module = nullptr;
    ze_module_build_log_handle_t buildLog = nullptr;
    ze_module_desc_t moduleDesc = {};
    moduleDesc.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.inputSize = spirv.size();
    moduleDesc.pInputModule = spirv.data();
    moduleDesc.pBuildFlags = "";
    CHECK_CALL( zeModuleCreate(context, hDevice, &moduleDesc, &module, &buildLog) );

    size_t buildLogLength = 0;
    CHECK_CALL( zeModuleBuildLogGetString(buildLog, &buildLogLength, nullptr) );

    std::string buildLogString(buildLogLength, ' ');
    CHECK_CALL( zeModuleBuildLogGetString(buildLog, &buildLogLength, &buildLogString[0]) );

    printf("Program build log for device: %s\n", deviceProps.name);
    printf("%s\n", buildLogString.c_str() );

    // kernel

    ze_kernel_handle_t kernel = nullptr;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
    kernelDesc.pKernelName = "inc_buffer";
    CHECK_CALL( zeKernelCreate(module, &kernelDesc, &kernel) );

    // destination

    int32_t* pDst = nullptr;

#ifdef USE_DEVICE_MEM
    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    CHECK_CALL( zeMemAllocDevice(context, &deviceAllocDesc, sizeof(int32_t), 0, hDevice, (void**)&pDst) );

    const int32_t zero = 0;
    CHECK_CALL( zeCommandListAppendMemoryFill(queue, pDst, &zero, sizeof(zero), sizeof(zero), nullptr, 0, nullptr) );
    CHECK_CALL( zeCommandListAppendBarrier(queue, nullptr, 0, nullptr) );
#else
    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_host_mem_alloc_desc_t hostAllocDesc = {};
    hostAllocDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    CHECK_CALL( zeMemAllocShared(context, &deviceAllocDesc, &hostAllocDesc, sizeof(int32_t), 0, hDevice, (void**)&pDst) );

    pDst[0] = 0;
#endif

    // kernel args

    CHECK_CALL( zeKernelSetArgumentValue(kernel, 0, sizeof(pDst), &pDst) );
    CHECK_CALL( zeKernelSetGroupSize(kernel, 1, 1, 1) );

    ze_group_count_t groupCount = {};
    groupCount.groupCountX = 1;
    groupCount.groupCountY = 1;
    groupCount.groupCountZ = 1;

    std::vector<ze_event_handle_t> events;
    events.reserve(numEvents);

    printf("Enqueuing kernels to create %zu events...\n", numEvents);
    for (int i = 0; i < numEvents; i++) {
        ze_event_handle_t event = nullptr;
        ze_event_desc_t eventDesc = {};
        eventDesc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
        eventDesc.index = i;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
        CHECK_CALL( zeEventCreate(eventPool, &eventDesc, &event) );

        CHECK_CALL( zeCommandListAppendLaunchKernel(queue, kernel, &groupCount, event, 0, nullptr) );
        CHECK_CALL( zeCommandListAppendBarrier(queue, nullptr, 0, nullptr) );

        events.push_back(event);
    }

    printf("Waiting for %zu kernels to complete...\n", numEvents);
    CHECK_CALL( zeCommandListHostSynchronize(queue, UINT64_MAX) );

    uint64_t totalTimeNS = 0;
    printf("Querying profiling data for %zu events...\n", numEvents);

    auto start = std::chrono::system_clock::now();

    for (const auto& event : events) {
        ze_kernel_timestamp_result_t ts = {};
        CHECK_CALL( zeEventQueryKernelTimestamp(event, &ts) );

        totalTimeNS += (ts.global.kernelEnd - ts.global.kernelStart);
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<float> queryTimeS = end - start;
    printf("Querying profiling data took %f s (%f us per event)\n",
        queryTimeS.count(), queryTimeS.count() * 1000000 / numEvents);

    int32_t result = -999;

#ifdef USE_DEVICE_MEM
    CHECK_CALL( zeCommandListAppendMemoryCopy(queue, &result, pDst, sizeof(result), nullptr, 0, nullptr) );
    CHECK_CALL( zeCommandListHostSynchronize(queue, UINT64_MAX) );
#else
    result = pDst[0];
#endif

    if (result == numEvents) {
        printf("Success.\n");
    } else {
        printf("Unexpected result: %d\n", result);
    }

    return 0;
}
