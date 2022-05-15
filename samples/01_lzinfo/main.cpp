/*
// Copyright (c) 2021 Ben Ashbaugh
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
*/

#include <stdio.h>
#include <vector>
#include <popl/popl.hpp>

#include "ze_api.h"
#include "zello_log.h"

int main(
    int argc,
    char** argv )
{
    {
        popl::OptionParser op("Supported Options");

        bool printUsage = false;
        try {
            op.parse(argc, argv);
        } catch (std::exception& e) {
            fprintf(stderr, "Error: %s\n\n", e.what());
            printUsage = true;
        }

        if (printUsage || !op.unknown_options().empty() || !op.non_option_args().empty()) {
            fprintf(stderr,
                "Usage: lzinfo [options]\n"
                "%s", op.help().c_str());
            return -1;
        }
    }

    ze_result_t result;

    result = zeInit(0);
    if (result != ZE_RESULT_SUCCESS) {
        printf("zeInit failed (%u)!\n", result);
    }

    uint32_t driverCount = 0;
    result = zeDriverGet(&driverCount, nullptr);
    printf("Enumerated %u drivers.\n\n", driverCount);

    std::vector<ze_driver_handle_t> drivers(driverCount);
    result = zeDriverGet(&driverCount, drivers.data());

    for (auto& driver : drivers) {
        printf("Driver:\n");

        ze_driver_properties_t driverProps = {};
        driverProps.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
        zeDriverGetProperties(driver, &driverProps);

        printf("\tDriver Version: %u\n", driverProps.driverVersion );

        uint32_t deviceCount = 0;
        zeDeviceGet(driver, &deviceCount, nullptr);

        std::vector<ze_device_handle_t> devices(deviceCount);
        zeDeviceGet(driver, &deviceCount, devices.data());

        for (uint32_t i = 0; i < deviceCount; i++) {
            printf("Device[%u]:\n", i);

            ze_device_properties_t deviceProps = {};
            deviceProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            zeDeviceGetProperties(devices[i], &deviceProps);

            ze_device_compute_properties_t computeProps = {};
            computeProps.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
            zeDeviceGetComputeProperties(devices[i], &computeProps);

            ze_device_module_properties_t moduleProps = {};
            moduleProps.stype = ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES;
            zeDeviceGetModuleProperties(devices[i], &moduleProps);

            //zeDeviceGetCommandQueueGroupProperties(

            //zeDeviceGetMemoryProperties

            ze_device_memory_access_properties_t memAccessProps = {};
            memAccessProps.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_ACCESS_PROPERTIES;
            zeDeviceGetMemoryAccessProperties(devices[i], &memAccessProps);

            //zeDeviceGetCacheProperties

            ze_device_image_properties_t imageProps = {};
            imageProps.stype = ZE_STRUCTURE_TYPE_IMAGE_PROPERTIES;
            zeDeviceGetImageProperties(devices[i], &imageProps);

            ze_device_external_memory_properties_t externalMemProps = {};
            externalMemProps.stype = ZE_STRUCTURE_TYPE_DEVICE_EXTERNAL_MEMORY_PROPERTIES;
            zeDeviceGetExternalMemoryProperties(devices[i], &externalMemProps);

            //zeDeviceGetP2PProperties

            printf("Device Properties:\n%s\n", to_string(deviceProps).c_str());
            printf("Compute Properties:\n%s\n", to_string(computeProps).c_str());
            printf("Module Properties:\n%s\n", to_string(moduleProps).c_str());
            printf("Memory Access Properties:\n%s\n", to_string(memAccessProps).c_str());
            printf("Image Properties:\n%s\n", to_string(imageProps).c_str());
            //printf("External Memory Properties:\n%s\n", to_string(externalMemProps).c_str());

            uint32_t queueGroupCount = 0;
            zeDeviceGetCommandQueueGroupProperties(devices[i], &queueGroupCount, nullptr);

            std::vector<ze_command_queue_group_properties_t> queueGroupProps(queueGroupCount);
            for (auto& prop : queueGroupProps) {
                prop.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
            }
            zeDeviceGetCommandQueueGroupProperties(devices[i], &queueGroupCount, queueGroupProps.data());

            for (uint32_t i = 0; i < queueGroupCount; i++) {
                printf("QueueGroup[%u]:\n", i);
                printf("Queue Group Properties:\n%s\n", to_string(queueGroupProps[i]).c_str());
            }

        }
        printf("\n");
    }

    printf( "Done.\n" );

    return 0;
}
