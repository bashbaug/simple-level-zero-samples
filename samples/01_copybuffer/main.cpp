/*
// Copyright (c) 2025 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include "ze_api.h"

#include <numeric>
#include <vector>

#include <popl/popl.hpp>

const size_t    gwx = 1024*1024;

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
                "Usage: copybuffer [options]\n"
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
    
    uint32_t* pDst = nullptr;
    ze_device_mem_alloc_desc_t deviceAllocDesc = {};
    deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_host_mem_alloc_desc_t hostAllocDesc = {};
    hostAllocDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    CHECK_CALL( zeMemAllocShared(context, &deviceAllocDesc, &hostAllocDesc, gwx * sizeof(uint32_t), 0, hDevice, (void**)&pDst) );

    uint32_t* pSrc = nullptr;
    CHECK_CALL( zeMemAllocShared(context, &deviceAllocDesc, &hostAllocDesc, gwx * sizeof(uint32_t), 0, hDevice, (void**)&pSrc) );

    std::iota(pSrc, pSrc + gwx, 0);

    CHECK_CALL( zeCommandListAppendMemoryCopy(queue, pDst, pSrc, gwx * sizeof(uint32_t), nullptr, 0, nullptr) );
    CHECK_CALL( zeCommandListHostSynchronize(queue, UINT64_MAX) );

    unsigned int mismatches = 0;
    for( size_t i = 0; i < gwx; i++ )
    {
        if( pDst[i] != i )
        {
            if( mismatches < 16 )
            {
                fprintf(stderr, "Mismatch!  dst[%d] == %08X, want %08X\n",
                    (unsigned int)i,
                    pDst[i],
                    (unsigned int)i );
            }
            mismatches++;
        }
    }
    if( mismatches )
    {
        fprintf(stderr, "Error: Found %d mismatches / %d values!!!\n",
            mismatches,
            (unsigned int)gwx );
    }
    else
    {
        printf("Success.\n");
    }

    CHECK_CALL( zeMemFree(context, pDst) );
    CHECK_CALL( zeMemFree(context, pSrc) );
    CHECK_CALL( zeCommandListDestroy(queue) );
    CHECK_CALL( zeContextDestroy(context) );

    printf("Done.\n");

    return 0;
}