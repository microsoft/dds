/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#include "FileBackEnd.h"

int
main(
    int Argc,
    char** Argv
) {
     int ret;

     ret = RunFileBackEnd(
        DDS_BACKEND_ADDR,
        DDS_BACKEND_PORT,
        1,
        1,
        Argc,
        Argv
    );

    return ret;
}
