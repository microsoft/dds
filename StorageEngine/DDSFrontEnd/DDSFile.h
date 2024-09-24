/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

#include "DDSFrontEndInterface.h"

namespace DDS_FrontEnd {

class DDSFile {
private:
    FileIdT Id;
    FileProperties Properties;

public:
    PollIdT PollId;
    ContextT PollContext;

public:
    DDSFile();

    DDSFile(
        FileIdT FileId,
        const char* FileName,
        FileAttributesT FileAttributes,
        FileAccessT FileAccess,
        FileShareModeT FileShareMode
    );

    const char*
    GetName();

    FileAttributesT
    GetAttributes();

    FileAccessT
    GetAccess();

    FileSizeT
    GetSize();

    FileSizeT
    GetPointer();

    FileShareModeT
    GetShareMode();

    time_t
    GetCreationTime();

    time_t
    GetLastAccessTime();

    time_t
    GetLastWriteTime();

    void
    SetName(
        const char* FileName
    );

    void
    SetSize(
        FileSizeT FileSize
    );

    void
    SetPointer(
        FileSizeT FilePointer
    );

    void
    IncrementPointer(
        FileSizeT Delta
    );

    void
    DecrementPointer(
        FileSizeT Delta
    );

    void
    SetLastAccessTime(
        time_t NewTime
    );

    void
    SetLastWriteTime(
        time_t NewTime
    );
};

}