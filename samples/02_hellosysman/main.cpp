/*
// Copyright (c) 2022 Ben Ashbaugh
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

#include <inttypes.h>
#include <stdio.h>
#include <vector>
#include <popl/popl.hpp>

#include "zes_api.h"

#if defined(_WIN32)
#define SETENV( _name, _value ) _putenv_s( _name, _value )
#else
#define SETENV( _name, _value ) setenv( _name, _value, 1 );
#endif

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

    SETENV("ZES_ENABLE_SYSMAN", "1");

    CHECK_CALL( zeInit(0) );

    uint32_t driverCount = 0;
    CHECK_CALL( zeDriverGet(&driverCount, nullptr) );
    printf("Enumerated %u drivers.\n\n", driverCount);

    std::vector<ze_driver_handle_t> drivers(driverCount);
    CHECK_CALL( zeDriverGet(&driverCount, drivers.data()) );

    for (auto& driver : drivers) {
        printf("Driver:\n");

        ze_driver_properties_t driverProps = {};
        driverProps.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
        CHECK_CALL( zeDriverGetProperties(driver, &driverProps) );

        uint32_t deviceCount = 0;
        CHECK_CALL( zeDeviceGet(driver, &deviceCount, nullptr) );

        std::vector<ze_device_handle_t> devices(deviceCount);
        CHECK_CALL( zeDeviceGet(driver, &deviceCount, devices.data()) );

        for (uint32_t i = 0; i < deviceCount; i++) {
            printf("Device[%u]:\n", i);

            ze_device_properties_t deviceProps = {};
            deviceProps.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            CHECK_CALL( zeDeviceGetProperties(devices[i], &deviceProps) );
            
            printf("\tname:           %s\n", deviceProps.name);
            printf("\tvendorId:       %04X\n", deviceProps.vendorId);
            printf("\tdeviceId:       %04X\n", deviceProps.deviceId);

            zes_device_handle_t hSDevice = (zes_device_handle_t)devices[i];

            uint32_t memoryCount = 0;
            CHECK_CALL( zesDeviceEnumMemoryModules(hSDevice, &memoryCount, nullptr) );

            printf("\tzesDeviceEnumMemoryModules returned %u memories.\n", memoryCount);

            std::vector<zes_mem_handle_t> memories(memoryCount);
            CHECK_CALL( zesDeviceEnumMemoryModules(hSDevice, &memoryCount, memories.data()) );

            for (auto& memory : memories) {
                printf("\tMemory:\n");

                zes_mem_state_t memState;
                memState.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                memState.pNext = nullptr;
                zesMemoryGetState(memory, &memState);

                printf("\t\tsize = %" PRIu64 "\n", memState.size);
                printf("\t\tfree = %" PRIu64 "\n", memState.free);
            }
        }
        printf("\n");
    }

    printf( "Done.\n" );

    return 0;
}
