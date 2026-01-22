/*
// Copyright (c) 2019-2025 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include <popl/popl.hpp>

#include "ze_api.h"

#include <fstream>
#include <string>
#include <vector>

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

int main(
    int argc,
    char** argv )
{
    int platformIndex = 0;
    int deviceIndex = 0;

    std::string fileName("sample_kernel64.spv");
    std::string kernelName("Test");
    std::string buildOptions;
    size_t gwx = 512;

    {
        popl::OptionParser op("Supported Options");
        op.add<popl::Value<int>>("p", "platform", "Platform Index", platformIndex, &platformIndex);
        op.add<popl::Value<int>>("d", "device", "Device Index", deviceIndex, &deviceIndex);
        op.add<popl::Value<std::string>>("", "file", "Kernel File Name", fileName, &fileName);
        op.add<popl::Value<std::string>>("", "name", "Kernel Name", kernelName, &kernelName);
        op.add<popl::Value<std::string>>("", "options", "Program Build Options", buildOptions, &buildOptions);
        op.add<popl::Value<size_t>>("", "gwx", "Global Work Size", gwx, &gwx);
        bool printUsage = false;
        try {
            op.parse(argc, argv);
        } catch (std::exception& e) {
            fprintf(stderr, "Error: %s\n\n", e.what());
            printUsage = true;
        }

        if (printUsage || !op.unknown_options().empty() || !op.non_option_args().empty()) {
            fprintf(stderr,
                "Usage: spirvkernelfromfile [options]\n"
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

    ze_context_handle_t context = nullptr;
    ze_context_desc_t contextDesc = {};
    contextDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    CHECK_CALL( zeContextCreate(drivers[platformIndex], &contextDesc, &context) );

    ze_command_list_handle_t queue = nullptr;
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    CHECK_CALL( zeCommandListCreateImmediate(context, hDevice, &queueDesc, &queue) );

    printf("Reading SPIR-V from file: %s\n", fileName.c_str());
    std::vector<uint8_t> spirv = readSPIRVFromFile(fileName);

    printf("Building program with build options: %s\n",
        buildOptions.empty() ? "(none)" : buildOptions.c_str() );
    ze_module_handle_t module = nullptr;
    ze_module_build_log_handle_t buildLog = nullptr;
    ze_module_desc_t moduleDesc = {};
    moduleDesc.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.inputSize = spirv.size();
    moduleDesc.pInputModule = spirv.data();
    moduleDesc.pBuildFlags = buildOptions.c_str();
    CHECK_CALL( zeModuleCreate(context, hDevice, &moduleDesc, &module, &buildLog) );

    size_t buildLogLength = 0;
    CHECK_CALL( zeModuleBuildLogGetString(buildLog, &buildLogLength, nullptr) );

    std::string buildLogString(buildLogLength, ' ');
    CHECK_CALL( zeModuleBuildLogGetString(buildLog, &buildLogLength, &buildLogString[0]) );

    printf("Program build log for device: %s\n", deviceProps.name);
    printf("%s\n", buildLogString.c_str() );

    printf("Creating kernel: %s\n", kernelName.c_str() );
    ze_kernel_handle_t kernel = nullptr;
    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
    kernelDesc.pKernelName = kernelName.c_str();
    CHECK_CALL( zeKernelCreate(module, &kernelDesc, &kernel) );

    uint32_t* pDst = nullptr;
    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    CHECK_CALL( zeMemAllocDevice(context, &deviceAllocDesc, gwx * sizeof(uint32_t), 0, hDevice, (void**)&pDst) );

    // execution
    CHECK_CALL( zeKernelSetArgumentValue(kernel, 0, sizeof(pDst), &pDst) );

    uint32_t lwx, lwy, lwz;
    CHECK_CALL( zeKernelSuggestGroupSize(kernel, gwx, 1, 1, &lwx, &lwy, &lwz) );
    CHECK_CALL( zeKernelSetGroupSize(kernel, lwx, 1, 1) );

    ze_group_count_t groupCount = {};
    groupCount.groupCountX = gwx / lwx;
    groupCount.groupCountY = 1;
    groupCount.groupCountZ = 1;
    CHECK_CALL( zeCommandListAppendLaunchKernel(queue, kernel, &groupCount, nullptr, 0, nullptr) );
    CHECK_CALL( zeCommandListAppendBarrier(queue, nullptr, 0, nullptr) );

    std::vector<uint32_t> pData(gwx);
    CHECK_CALL( zeCommandListAppendMemoryCopy(queue, pData.data(), pDst, gwx * sizeof(uint32_t), nullptr, 0, nullptr) );
    CHECK_CALL( zeCommandListHostSynchronize(queue, UINT64_MAX) );

    if (gwx > 3) {
        printf("First few values:\n"
            " [0] = 0x%08X (as hex) = %u (as int) = %.2f (as float)\n"
            " [1] = 0x%08X (as hex) = %u (as int) = %.2f (as float)\n"
            " [2] = 0x%08X (as hex) = %u (as int) = %.2f (as float)\n",
            pData[0], pData[0], *((float*)&pData[0]),
            pData[1], pData[1], *((float*)&pData[1]),
            pData[2], pData[2], *((float*)&pData[2]));
    }

    printf("Done.\n");

    return 0;
}
