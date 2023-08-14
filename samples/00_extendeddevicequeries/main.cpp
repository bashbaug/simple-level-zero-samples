/*
// Copyright (c) 2023 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#include <stdio.h>
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

            uint32_t subDeviceCount = 0;
            zeDeviceGetSubDevices(devices[i], &subDeviceCount, nullptr);
            printf("\tnumSubDevices:  %u\n", subDeviceCount);

            printf("\tnumSlices:      %u\n", deviceProps.numSlices);
            printf("\tnumSubSlices:   %u\n", deviceProps.numSlices * deviceProps.numSubslicesPerSlice);
            printf("\tnumEUs:         %u\n", deviceProps.numSlices * deviceProps.numSubslicesPerSlice * deviceProps.numEUsPerSubslice);

            printf("\tdeviceuuid:     %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
                deviceProps.uuid.id[0], deviceProps.uuid.id[1], deviceProps.uuid.id[2], deviceProps.uuid.id[3],
                deviceProps.uuid.id[4], deviceProps.uuid.id[5], deviceProps.uuid.id[6], deviceProps.uuid.id[7],
                deviceProps.uuid.id[8], deviceProps.uuid.id[9], deviceProps.uuid.id[10], deviceProps.uuid.id[11],
                deviceProps.uuid.id[12], deviceProps.uuid.id[13], deviceProps.uuid.id[14], deviceProps.uuid.id[15]);
        }
        printf("\n");
    }

    printf( "Done.\n" );

    return 0;
}
