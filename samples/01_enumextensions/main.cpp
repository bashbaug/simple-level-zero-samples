/*
// Copyright (c) 2024 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include <stdio.h>
#include <algorithm>
#include <vector>
#include <popl/popl.hpp>

#include "ze_api.h"

static void PrintDeviceType(
    const char* label,
    ze_device_type_t type )
{
    const char* s;
    switch (type) {
        case ZE_DEVICE_TYPE_GPU: s = "GPU"; break;
        case ZE_DEVICE_TYPE_CPU: s = "CPU"; break;
        case ZE_DEVICE_TYPE_FPGA: s = "FPGA"; break;
        case ZE_DEVICE_TYPE_MCA: s = "MCA"; break;
        default: s = "*UNKNOWN!"; break;
    }
    printf("%s%s\n", label, s);
}

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
                "Usage: enumlevelzero [options]\n"
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

            PrintDeviceType("\ttype:           ", deviceProps.type);

            printf("\tname:           %s\n", deviceProps.name);
            printf("\tvendorId:       %04X\n", deviceProps.vendorId);
            printf("\tdeviceId:       %04X\n", deviceProps.deviceId);
        }

        uint32_t extensionCount = 0;
        zeDriverGetExtensionProperties(driver, &extensionCount, nullptr);

        std::vector<ze_driver_extension_properties_t> extensions(extensionCount);
        zeDriverGetExtensionProperties(driver, &extensionCount, extensions.data());

        std::sort(extensions.begin(), extensions.end(),
            [](const ze_driver_extension_properties_t& a, const ze_driver_extension_properties_t& b) {
                return strcmp(a.name, b.name) < 0;
            });

        printf("Extension:\n");
        for (auto& extension : extensions) {
            printf("\t%s (%u.%u)\n", extension.name, ZE_MAJOR_VERSION(extension.version), ZE_MINOR_VERSION(extension.version));
        }
        printf("\n");
    }

    printf( "Done.\n" );

    return 0;
}
