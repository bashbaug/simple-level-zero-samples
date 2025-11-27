/*
// Copyright (c) 2025 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include "ze_api.h"

#include <iostream>
#include <vector>
#include <cstring>

#define CHECK(res, msg) \
    if(res != ZE_RESULT_SUCCESS) { \
        std::cerr << "[ERROR] " << msg << " (error=" << res << ")\n"; \
        return 1; \
    } else { \
        std::cout << "[OK] " << msg << "\n"; \
    }

int main() {
    std::cout << "=== Level Zero Buffer Copy Sample ===\n";

    // 1. Initialize Level Zero
    CHECK(zeInit(ZE_INIT_FLAG_GPU_ONLY), "zeInit");

    // 2. Pick driver and device
    uint32_t driverCount = 0;
    zeDriverGet(&driverCount, nullptr);
    std::vector<ze_driver_handle_t> drivers(driverCount);
    zeDriverGet(&driverCount, drivers.data());
    if (driverCount == 0) {
        std::cerr << "No Level Zero drivers found!\n";
        return 1;
    }

    ze_driver_handle_t driver = drivers[0];

    uint32_t deviceCount = 0;
    zeDeviceGet(driver, &deviceCount, nullptr);
    std::vector<ze_device_handle_t> devices(deviceCount);
    zeDeviceGet(driver, &deviceCount, devices.data());
    if (deviceCount == 0) {
        std::cerr << "No Level Zero devices found!\n";
        return 1;
    }

    ze_device_handle_t device = devices[0];

    // 3. Create context
    ze_context_handle_t context;
    ze_context_desc_t ctxDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    CHECK(zeContextCreate(driver, &ctxDesc, &context), "zeContextCreate");

    // 4. Create queue and list
    ze_command_queue_handle_t queue;
    ze_command_queue_desc_t qdesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    CHECK(zeCommandQueueCreate(context, device, &qdesc, &queue), "zeCommandQueueCreate");

    ze_command_list_handle_t list;
    ze_command_list_desc_t ldesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    ldesc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
    CHECK(zeCommandListCreate(context, device, &ldesc, &list), "zeCommandListCreate");

    // 5. Allocate device buffer
    const size_t N = 4;
    size_t bytes = N * sizeof(int);

    ze_device_mem_alloc_desc_t dalloc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    int* dBuffer;
    CHECK(zeMemAllocDevice(context, &dalloc, bytes, /*align*/1, device, (void**)&dBuffer),
          "zeMemAllocDevice");

    // 6. Prepare host buffer
    int hostIn[N], hostOut[N];
    for (size_t i = 0; i < N; i++)
        hostIn[i] = (int)(i * 10);

    // 7. Copy host → device buffer
    CHECK(zeCommandListAppendMemoryCopy(
              list,
              dBuffer,
              hostIn,
              bytes,
              nullptr, 0, nullptr),
          "Copy host → device");

    // 8. Copy device → host buffer
    CHECK(zeCommandListAppendMemoryCopy(
              list,
              hostOut,
              dBuffer,
              bytes,
              nullptr, 0, nullptr),
          "Copy device → host");

    // 9. Execute commands
    CHECK(zeCommandListClose(list), "zeCommandListClose");
    CHECK(zeCommandQueueExecuteCommandLists(queue, 1, &list, nullptr),
          "zeCommandQueueExecuteCommandLists");
    CHECK(zeCommandQueueSynchronize(queue, UINT64_MAX),
          "zeCommandQueueSynchronize");

    // 10. Print results
    std::cout << "\n=== Result from device Buffer ===\n";
    for (size_t i = 0; i < N; i++)
        std::cout << "HostIn[" << i << "] = " << hostIn[i] << "   HostOut[" << i << "] = " << hostOut[i] << "\n";

    // 11. Cleanup
    zeMemFree(context, dBuffer);
    zeCommandListDestroy(list);
    zeCommandQueueDestroy(queue);
    zeContextDestroy(context);

    std::cout << "\n=== DONE ===\n";
    return 0;
}
