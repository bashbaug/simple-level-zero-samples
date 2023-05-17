/*
// Copyright (c) 2023 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include <popl/popl.hpp>

#include "ze_api.h"

#include <cinttypes>

#include "util.hpp"

int main(
    int argc,
    char** argv )
{
    constexpr size_t size = 1024 * 1024;
    int platformIndex = 0;
    int deviceIndex = 0;

    {
        popl::OptionParser op("Supported Options");
        op.add<popl::Value<int>>("p", "platform", "Platform Index", platformIndex, &platformIndex);
        op.add<popl::Value<int>>("d", "device", "Device Index", deviceIndex, &deviceIndex);

        bool printUsage = false;
        try {
            op.parse(argc, argv);
        } catch (std::exception& e) {
            fprintf(stderr, "Error: %s\n\n", e.what());
            printUsage = true;
        }

        if (printUsage || !op.unknown_options().empty() || !op.non_option_args().empty()) {
            fprintf(stderr,
                "Usage: handlequery [options]\n"
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

    ze_device_external_memory_properties_t externalMemProps = {};
    externalMemProps.stype = ZE_STRUCTURE_TYPE_DEVICE_EXTERNAL_MEMORY_PROPERTIES;
    CHECK_CALL( zeDeviceGetExternalMemoryProperties(devices[deviceIndex], &externalMemProps) );

    printf("Memory Import Types: %08X\n", externalMemProps.memoryAllocationImportTypes);
    printf("Memory Export Types: %08X\n", externalMemProps.memoryAllocationExportTypes);
    printf("Image Import Types: %08X\n", externalMemProps.imageImportTypes);
    printf("Image Export Types: %08X\n", externalMemProps.imageExportTypes);

    ze_context_handle_t context = nullptr;
    ze_context_desc_t contextDesc = {};
    contextDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    CHECK_CALL( zeContextCreate(drivers[platformIndex], &contextDesc, &context) );

    uint32_t* pDst = nullptr;
    ze_external_memory_export_desc_t exportDesc = {};
    exportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceAllocDesc.pNext = &exportDesc;
    CHECK_CALL( zeMemAllocDevice(context, &deviceAllocDesc, size, 0, devices[deviceIndex], (void**)&pDst) );

    ze_external_memory_export_win32_handle_t exportWin32Handle = {};
    exportWin32Handle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_WIN32;
    exportWin32Handle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;
    ze_memory_allocation_properties_t allocProps = {};
    allocProps.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
    allocProps.pNext = &exportWin32Handle;
    CHECK_CALL( zeMemGetAllocProperties(context, pDst, &allocProps, nullptr ) );

    printf("Queried external memory handle %p.\n", exportWin32Handle.handle);

    CHECK_CALL( zeMemFree( context, pDst ) );

    printf("All done!\n");
    return 0;
}