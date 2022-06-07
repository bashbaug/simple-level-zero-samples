/*
// Copyright (c) 2022 Ben Ashbaugh
//
// SPDX-License-Identifier: MIT
*/

#pragma once

#include "ze_api.h"

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
