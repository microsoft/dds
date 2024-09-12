/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#include <string.h>

#include "DDSFile.h"

namespace DDS_FrontEnd {

DDSFile::DDSFile() {
    memset(this->Properties.FileName, 0, DDS_MAX_FILE_PATH);

    this->Id = DDS_FILE_INVALID;
    this->Properties.FileAttributes = 0;
    this->Properties.CreationTime = time(NULL);
    this->Properties.LastAccessTime = time(NULL);
    this->Properties.LastWriteTime = time(NULL);
    this->Properties.FileSize = 0;
    this->Properties.Position = 0;
    this->Properties.Access = 0;
    this->Properties.ShareMode = 0;
    this->PollId = DDS_POLL_DEFAULT;
    this->PollContext = nullptr;
}

DDSFile::DDSFile(
    FileIdT FileId,
    const char* FileName,
    FileAttributesT FileAttributes,
    FileAccessT FileAccess,
    FileShareModeT FileShareMode
) {
    memset(this->Properties.FileName, 0, DDS_MAX_FILE_PATH);

    this->Id = FileId;
    strcpy(this->Properties.FileName, FileName);
    this->Properties.FileAttributes = FileAttributes;
    this->Properties.CreationTime = time(NULL);
    this->Properties.LastAccessTime = time(NULL);
    this->Properties.LastWriteTime = time(NULL);
    this->Properties.FileSize = 0;
    this->Properties.Position = 0;
    this->Properties.Access = FileAccess;
    this->Properties.ShareMode = FileShareMode;
    this->PollId = DDS_POLL_DEFAULT;
    this->PollContext = nullptr;
}

const char*
DDSFile::GetName() {
    return this->Properties.FileName;
}

FileAttributesT
DDSFile::GetAttributes() {
    return this->Properties.FileAttributes;
}

FileAccessT
DDSFile::GetAccess() {
    return this->Properties.Access;
}

FileShareModeT
DDSFile::GetShareMode() {
    return this->Properties.ShareMode;
}

FileSizeT
DDSFile::GetSize() {
    return this->Properties.FileSize;
}

FileSizeT
DDSFile::GetPointer() {
    return this->Properties.Position;
}

time_t
DDSFile::GetCreationTime() {
    return this->Properties.CreationTime;
}

time_t
DDSFile::GetLastAccessTime() {
    return this->Properties.LastAccessTime;
}

time_t
DDSFile::GetLastWriteTime() {
    return this->Properties.LastWriteTime;
}

void
DDSFile::SetName(
    const char* FileName
) {
    strcpy(this->Properties.FileName, FileName);
}

void
DDSFile::SetSize(FileSizeT FileSize) {
    this->Properties.FileSize = FileSize;
}

void
DDSFile::SetPointer(FileSizeT FilePointer) {
    this->Properties.Position.store(FilePointer, std::memory_order_relaxed);
}

void
DDSFile::IncrementPointer(FileSizeT Delta) {
    PositionInFileT delta = Delta;
    PositionInFileT currentPointer = this->Properties.Position;
    while(this->Properties.Position.compare_exchange_weak(currentPointer, currentPointer + delta, std::memory_order_relaxed) == false) {
        currentPointer = this->Properties.Position;
    }
}

void
DDSFile::DecrementPointer(FileSizeT Delta) {
    PositionInFileT delta = Delta;
    PositionInFileT currentPointer = this->Properties.Position;
    while(this->Properties.Position.compare_exchange_weak(currentPointer, currentPointer - delta, std::memory_order_relaxed) == false) {
        currentPointer = this->Properties.Position;
    }
}

void
DDSFile::SetLastAccessTime(
    time_t NewTime
) {
    this->Properties.LastAccessTime = NewTime;
}

void
DDSFile::SetLastWriteTime(
    time_t NewTime
) {
    this->Properties.LastWriteTime = NewTime;
}

}