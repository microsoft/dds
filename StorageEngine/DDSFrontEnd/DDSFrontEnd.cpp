/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#include <chrono>
#include <iostream>
#include <string.h>
#include <thread>

#include "DDSFrontEnd.h"

using std::cout;
using std::endl;

namespace DDS_FrontEnd {

//
// Initialize a poll structure
//
//
PollT::PollT() {
    for (size_t i = 0; i != DDS_MAX_OUTSTANDING_IO; i++) {
        OutstandingRequests[i] = nullptr;
    }
#if BACKEND_TYPE == BACKEND_TYPE_DPU
    MsgBuffer = NULL;
    RequestRing = NULL;
    ResponseRing = NULL;
#endif
}

#if BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Set up the DMA buffer
//
//
ErrorCodeT PollT::SetUpDMABuffer(void* BackEnd) {
    DDSBackEndBridge* backEndDPU = (DDSBackEndBridge*)BackEnd;
    MsgBuffer = new DMABuffer(DDS_BACKEND_ADDR, DDS_BACKEND_PORT, DDS_REQUEST_RING_BYTES + DDS_RESPONSE_RING_BYTES + 1024, backEndDPU->ClientId);
    if (!MsgBuffer) {
        cout << __func__ << " [error]: Failed to allocate a DMABuffer object" << endl;
        return DDS_ERROR_CODE_OOM;
    }

    bool allocResult = MsgBuffer->Allocate(&backEndDPU->LocalSock, &backEndDPU->BackEndSock, backEndDPU->QueueDepth, backEndDPU->MaxSge, backEndDPU->InlineThreshold);
    if (!allocResult) {
        cout << __func__ << " [error]: Failed to allocate DMA buffer" << endl;
        delete MsgBuffer;
        MsgBuffer = NULL;
        return DDS_ERROR_CODE_FAILED_BUFFER_ALLOCATION;
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Destroy the DMA buffer
//
//
void PollT::DestroyDMABuffer() {
    if (MsgBuffer) {
        MsgBuffer->Release();
        delete MsgBuffer;
        MsgBuffer = NULL;
    }
}

//
// Initialize request and response rings
//
//
void PollT::InitializeRings() {
    RequestRing = AllocateRequestBufferProgressive(MsgBuffer->BufferAddress);
    ResponseRing = AllocateResponseBufferProgressive(RequestRing->Buffer + DDS_REQUEST_RING_BYTES);
}
#endif
    
//
// A dummy callback for app
//
//
void
DummyReadWriteCallback(
    ErrorCodeT ErrorCode,
    FileIOSizeT BytesServiced,
    ContextT Context
) { }

DDSFrontEnd::DDSFrontEnd(
    const char* StoreName
) : BackEnd(NULL) {
    //
    // Set the name of the store
    //
    //
    strcpy(this->StoreName, StoreName);

    //
    // Prepare directories, files, and polls
    //
    //
    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        AllDirs[d] = nullptr;
    }
    DirIdEnd = 0;

    for (size_t f = 0; f != DDS_MAX_FILES; f++) {
        AllFiles[f] = nullptr;
    }
    FileIdEnd = 0;

    for (size_t p = 0; p != DDS_MAX_POLLS; p++) {
        AllPolls[p] = nullptr;
    }
    PollIdEnd = 0;

    //
    // Cannot allocate heap here otherwise SQL server would crash
    //
}

DDSFrontEnd::~DDSFrontEnd() {
    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        if (AllDirs[d] != nullptr) {
            delete AllDirs[d];
        }
    }

    for (size_t f = 0; f != DDS_MAX_FILES; f++) {
        if (AllFiles[f] != nullptr) {
            delete AllFiles[f];
        }
    }

    for (size_t p = 0; p != DDS_MAX_POLLS; p++) {
        if (AllPolls[p]) {
#if BACKEND_TYPE == BACKEND_TYPE_DPU
            AllPolls[p]->DestroyDMABuffer();
#endif
            for (size_t i = 0; i != DDS_MAX_OUTSTANDING_IO; i++) {
                delete AllPolls[p]->OutstandingRequests[i];
            }

            delete AllPolls[p];
        }
    }

    if (BackEnd) {
        BackEnd->Disconnect();
        delete BackEnd;
    }
}

//
// Intialize the front end, including connecting to back end
// and setting up the root directory and default poll
//
//
ErrorCodeT
DDSFrontEnd::Initialize() {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;
        
    //
    // Set up back end
    //
    //
#if BACKEND_TYPE == BACKEND_TYPE_DPU
    BackEnd = new DDSBackEndBridge();
#elif BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
    BackEnd = new DDSBackEndBridgeForLocalMemory();
#else
#error "Unknown backend type"
#endif
    result = BackEnd->Connect();
    if (result != DDS_ERROR_CODE_SUCCESS) {
        cout << __func__ << " [error]: Failed to connect to the back end (" << result << ")" << endl;
        return result;
    }

    //
    // Set up root directory
    //
    //
    DDSDir* rootDir = new DDSDir(DDS_DIR_ROOT, DDS_DIR_INVALID, "/");
    AllDirs[DDS_DIR_ROOT] = rootDir;

    //
    // Set up default poll
    //
    //
    PollT* poll = new PollT();
    for (size_t i = 0; i != DDS_MAX_OUTSTANDING_IO; i++) {
        poll->OutstandingRequests[i] = new FileIOT();
        if (!poll->OutstandingRequests[i]) {
            fprintf(stderr, "%s [error]: Failed to allocate a file I/O object\n", __func__);
            for (size_t j = 0; j != i; j++) {
                delete poll->OutstandingRequests[j];
            }
            return DDS_ERROR_CODE_OOM;
        }

        poll->OutstandingRequests[i]->RequestId = (RequestIdT)i;
    }
    poll->NextRequestSlot = 0;
    AllPolls[DDS_POLL_DEFAULT] = poll;
#if BACKEND_TYPE == BACKEND_TYPE_DPU
    DDSBackEndBridge* backEndDPU = (DDSBackEndBridge*)BackEnd;
    poll->SetUpDMABuffer(backEndDPU);
    poll->InitializeRings();
    fprintf(stdout, "%s [info]: Request ring data base address = %p\n", __func__, poll->RequestRing->Buffer);
    fprintf(stdout, "%s [info]: Response ring data base address = %p\n", __func__, poll->ResponseRing->Buffer);
#endif

    return result;
}

//
// Create a diretory
// 
//
ErrorCodeT
DDSFrontEnd::CreateDirectory(
    const char* PathName,
    DirIdT* DirId
) {
    //
    // Check existance of the path
    //
    //
    bool exists = false;
    for (size_t i = 0; i != DirIdEnd; i++) {
        if (AllDirs[i] && strcmp(PathName, AllDirs[i]->GetName()) == 0) {
            exists = true;
            break;
        }
    }
    if (exists) {
        return DDS_ERROR_CODE_DIR_EXISTS;
    }

    //
    // Retrieve an id
    //
    //
    DirIdT id = 0;
    for (; id != DDS_MAX_DIRS; id++) {
        if (!AllDirs[id]) {
            break;
        }
    }

    if (id == DDS_MAX_DIRS) {
        return DDS_ERROR_CODE_TOO_MANY_DIRS;
    }

    DirIdT parentId = DDS_DIR_ROOT;

    DDSDir* dir = new DDSDir(id, parentId, PathName);
    AllDirs[parentId]->AddChild(id);

    AllDirs[id] = dir;
    if (id >= DirIdEnd) {
        //
        // End is the next to the last valid directory ID
        //
        //
        DirIdEnd = id + 1;
    }

    *DirId = id;

    //
    // Reflect the update on back end
    //
    //
    return BackEnd->CreateDirectory(PathName, *DirId, parentId);
}

void
DDSFrontEnd::RemoveDirectoryRecursive(
    DDSDir* Dir
) {
    LinkedList<DirIdT>* pChildren = Dir->GetChildren();
    for (auto cur = pChildren->Begin(); cur != pChildren->End(); cur = cur->Next) {
        RemoveDirectoryRecursive(AllDirs[cur->Value]);
        delete AllDirs[cur->Value];
        AllDirs[cur->Value] = nullptr;
    }
    pChildren->DeleteAll();

    LinkedList<FileIdT>* pFiles = Dir->GetFiles();
    for (auto cur = pFiles->Begin(); cur != pFiles->End(); cur = cur->Next) {
        delete AllFiles[cur->Value];
        AllFiles[cur->Value] = nullptr;
    }
    pFiles->DeleteAll();
}

//
// Delete a directory
// 
//
ErrorCodeT
DDSFrontEnd::RemoveDirectory(
    const char* PathName
) {
    DirIdT id = DDS_DIR_INVALID;

    for (DirIdT i = 0; i != DirIdEnd; i++) {
        if (AllDirs[i] && strcmp(PathName, AllDirs[i]->GetName()) == 0) {
            id = i;
            break;
        }
    }

    if (id == DDS_DIR_INVALID) {
        return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }

    //
    // Recursively delete all files and child directories
    //
    //
    RemoveDirectoryRecursive(AllDirs[id]);

    delete AllDirs[id];
    AllDirs[id] = nullptr;

    //
    // Reflect the update on back end
    //
    //
    return BackEnd->RemoveDirectory(id);
}

//
// Create a file
// 
//
ErrorCodeT
DDSFrontEnd::CreateFile(
    const char* FileName,
    FileAccessT DesiredAccess,
    FileShareModeT ShareMode,
    FileAttributesT FileAttributes,
    FileIdT* FileId
) {
    //
    // Check existance of the file
    //
    //
    bool exists = false;
    for (size_t i = 0; i != FileIdEnd; i++) {
        if (AllFiles[i] && strcmp(FileName, AllFiles[i]->GetName()) == 0) {
            exists = true;
            break;
        }
    }
    if (exists) {
        return DDS_ERROR_CODE_FILE_EXISTS;
    }

    //
    // Retrieve an id
    //
    //
    FileIdT id = 0;
    for (; id != DDS_MAX_FILES; id++) {
        if (!AllFiles[id]) {
            break;
        }
    }

    if (id == DDS_MAX_FILES) {
        return DDS_ERROR_CODE_TOO_MANY_FILES;
    }

    //
    // Create the file
    //
    //
    DDSFile* file = new DDSFile(id, FileName, FileAttributes, DesiredAccess, ShareMode);

    DirIdT dirId = DDS_DIR_ROOT;
    AllDirs[dirId]->AddFile(id);

    AllFiles[id] = file;
    if (id >= FileIdEnd) {
        //
        // End is the next to the last valid file ID
        //
        //
        FileIdEnd = id + 1;
    }

    *FileId = id;

    //
    // Reflect the update on back end
    //
    //
    return BackEnd->CreateFile(FileName, FileAttributes, *FileId, dirId);
}

//
// Delete a file
// 
//
ErrorCodeT
DDSFrontEnd::DeleteFile(
    const char* FileName
) {
    FileIdT id = DDS_FILE_INVALID;

    for (FileIdT i = 0; i != FileIdEnd; i++) {
        if (AllFiles[i] && strcmp(FileName, AllFiles[i]->GetName()) == 0) {
            id = i;
            break;
        }
    }

    if (id == DDS_FILE_INVALID) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    DirIdT dirId = DDS_DIR_ROOT;
    AllDirs[dirId]->DeleteFile(id);

    delete AllFiles[id];
    AllFiles[id] = nullptr;

    //
    // Reflect the update on back end
    //
    //
    return BackEnd->DeleteFile(id, dirId);
}

//
// Change the size of a file
// 
//
ErrorCodeT
DDSFrontEnd::ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize
) {
    //
    // Change file size on back end first
    //
    //
    ErrorCodeT result = BackEnd->ChangeFileSize(FileId, NewSize);
    
    if (result == DDS_ERROR_CODE_SUCCESS) {
        AllFiles[FileId]->SetSize(NewSize);
    }

    return result;
}

//
// Set the physical file size for the specified file
// to the current position of the file pointer
// 
//
ErrorCodeT
DDSFrontEnd::SetEndOfFile(
    FileIdT FileId
) {
    DDSFile* pFile = AllFiles[FileId];

    //
    // Change file size on back end first
    //
    //
    ErrorCodeT result = BackEnd->ChangeFileSize(FileId, pFile->GetPointer());
    
    if (result == DDS_ERROR_CODE_SUCCESS) {
        pFile->SetSize(pFile->GetPointer());
    }

    return result;
}

//
// Move the file pointer of the specified file
// 
//
#pragma warning(disable:26812)
ErrorCodeT
DDSFrontEnd::SetFilePointer(
    FileIdT FileId,
    PositionInFileT DistanceToMove,
    FilePointerPosition MoveMethod
) {
    DDSFile* pFile = AllFiles[FileId];
    PositionInFileT newPosition = 0;

    switch (MoveMethod)
    {
    case FilePointerPosition::BEGIN:
        newPosition = DistanceToMove;
        break;
    case FilePointerPosition::CURRENT:
        newPosition = pFile->GetPointer() + DistanceToMove;
        break;
    case FilePointerPosition::END:
        newPosition = pFile->GetSize() + DistanceToMove;
        break;
    default:
        return DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
    }

    pFile->SetPointer(newPosition);

    return DDS_ERROR_CODE_SUCCESS;
}
#pragma warning(default:26812)

//
// Get file size
// 
//
ErrorCodeT
DDSFrontEnd::GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize
) {
    if (FileId >= DDS_MAX_FILES || AllFiles[FileId] == nullptr) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    //
    // No writes on the DPU, so it's safe to get file size here
    //
    //
    *FileSize = AllFiles[FileId]->GetSize();

    return DDS_ERROR_CODE_SUCCESS;
}

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async read from a file
// 
//
ErrorCodeT
DDSFrontEnd::ReadFile(
    FileIdT FileId,
    BufferT DestBuffer,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->AppBuffer = DestBuffer;
    pIO->AppBufferArray = nullptr;
    pIO->BytesDesired = BytesToRead;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFile(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBuffer,
        pIO->BytesDesired,
        BytesRead,
        pIO,
        poll
    );

    //
    // TODO: better handle file pointer
    //
    //
    FileSizeT newPointer = ((DDSFile*)pIO->FileReference)->GetPointer() + BytesToRead;
    ((DDSFile*)pIO->FileReference)->SetPointer(newPointer);

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async read from a file
// 
//
ErrorCodeT
DDSFrontEnd::ReadFile(
    FileIdT FileId,
    BufferT DestBuffer,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }
        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->BytesDesired = BytesToRead;
    pIO->AppBuffer = DestBuffer;
    pIO->AppBufferArray = nullptr;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFile(
        FileId,
        pIO->Offset,
        DestBuffer,
        BytesToRead,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    //
    // Update file pointer
    //
    //
    ((DDSFile*)pIO->FileReference)->IncrementPointer(BytesToRead);

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async read from a file with scattering
// 
//
ErrorCodeT
DDSFrontEnd::ReadFileScatter(
    FileIdT FileId,
    BufferT* DestBufferArray,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = DestBufferArray;
    pIO->BytesDesired = BytesToRead;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFileScatter(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBufferArray,
        pIO->BytesDesired,
        BytesRead,
        pIO,
        poll
    );

    FileSizeT newPointer = ((DDSFile*)pIO->FileReference)->GetPointer() + BytesToRead;
    ((DDSFile*)pIO->FileReference)->SetPointer(newPointer);

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async read from a file with scattering
// 
//
ErrorCodeT
DDSFrontEnd::ReadFileScatter(
    FileIdT FileId,
    BufferT* DestBufferArray,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }

        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->BytesDesired = BytesToRead;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = DestBufferArray;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFileScatter(
        FileId,
        pIO->Offset,
        DestBufferArray,
        BytesToRead,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    //
    // Update file pointer
    //
    //
    ((DDSFile*)pIO->FileReference)->IncrementPointer(BytesToRead);

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async write to a file
// 
//
ErrorCodeT
DDSFrontEnd::WriteFile(
    FileIdT FileId,
    BufferT SourceBuffer,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = false;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->AppBuffer = DestBuffer;
    pIO->AppBufferArray = nullptr;
    pIO->BytesDesired = BytesToWrite;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFile(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBuffer,
        pIO->BytesDesired,
        BytesWritten,
        pIO,
        poll
    );

    FileSizeT newPointer = ((DDSFile*)pIO->FileReference)->GetPointer() + BytesToWrite;
    ((DDSFile*)pIO->FileReference)->SetPointer(newPointer);

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async write to a file
// 
//
ErrorCodeT
DDSFrontEnd::WriteFile(
    FileIdT FileId,
    BufferT SourceBuffer,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }

        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = false;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->BytesDesired = BytesToWrite;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = nullptr;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFile(
        FileId,
        pIO->Offset,
        SourceBuffer,
        BytesToWrite,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    //
    // Update file pointer
    //
    //
    ((DDSFile*)pIO->FileReference)->IncrementPointer(BytesToWrite);

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async write to a file with gathering
// 
//
ErrorCodeT
DDSFrontEnd::WriteFileGather(
    FileIdT FileId,
    BufferT* SourceBufferArray,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = false;
    pIO->IsSegmented = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = SourceBufferArray;
    pIO->BytesDesired = BytesToWrite;
    pIO->BytesServiced = 0;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFileGather(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBufferArray,
        pIO->BytesDesired,
        &pIO->BytesServiced,
        pIO,
        poll
    );

    if (BytesWritten) {
        *BytesWritten = pIO->BytesServiced;
    }

    FileSizeT newPointer = ((DDSFile*)pIO->FileReference)->GetPointer() + BytesToWrite;
    ((DDSFile*)pIO->FileReference)->SetPointer(newPointer);

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async write to a file with gathering
// 
//
ErrorCodeT
DDSFrontEnd::WriteFileGather(
    FileIdT FileId,
    BufferT* SourceBufferArray,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }
        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = false;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = AllFiles[FileId]->GetPointer();
    pIO->BytesDesired = BytesToWrite;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = nullptr;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFileGather(
        FileId,
        pIO->Offset,
        SourceBufferArray,
        BytesToWrite,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    //
    // Update file pointer
    //
    //
    ((DDSFile*)pIO->FileReference)->IncrementPointer(BytesToWrite);

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

//
// Read/write with offset
//
//

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async read from a file
// 
//
ErrorCodeT
DDSFrontEnd::ReadFile(
    FileIdT FileId,
    BufferT DestBuffer,
    FileSizeT Offset,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->AppBuffer = DestBuffer;
    pIO->AppBufferArray = nullptr;
    pIO->BytesDesired = BytesToRead;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFile(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBuffer,
        pIO->BytesDesired,
        BytesRead,
        pIO,
        poll
    );

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async read from a file
// 
//
ErrorCodeT
DDSFrontEnd::ReadFile(
    FileIdT FileId,
    BufferT DestBuffer,
    FileSizeT Offset,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }
        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->BytesDesired = BytesToRead;
    pIO->AppBuffer = DestBuffer;
    pIO->AppBufferArray = nullptr;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFile(
        FileId,
        Offset,
        DestBuffer,
        BytesToRead,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async read from a file with scattering
// 
//
ErrorCodeT
DDSFrontEnd::ReadFileScatter(
    FileIdT FileId,
    BufferT* DestBufferArray,
    FileSizeT Offset,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = DestBufferArray;
    pIO->BytesDesired = BytesToRead;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFileScatter(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBufferArray,
        pIO->BytesDesired,
        BytesRead,
        pIO,
        poll
    );

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async read from a file with scattering
// 
//
ErrorCodeT
DDSFrontEnd::ReadFileScatter(
    FileIdT FileId,
    BufferT* DestBufferArray,
    FileSizeT Offset,
    FileIOSizeT BytesToRead,
    FileIOSizeT* BytesRead,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }

        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->BytesDesired = BytesToRead;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = DestBufferArray;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->ReadFileScatter(
        FileId,
        Offset,
        DestBufferArray,
        BytesToRead,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async write to a file
// 
//
ErrorCodeT
DDSFrontEnd::WriteFile(
    FileIdT FileId,
    BufferT SourceBuffer,
    FileSizeT Offset,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = false;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->AppBuffer = DestBuffer;
    pIO->AppBufferArray = nullptr;
    pIO->BytesDesired = BytesToWrite;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFile(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBuffer,
        pIO->BytesDesired,
        BytesWritten,
        pIO,
        poll
    );

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async write to a file
// 
//
ErrorCodeT
DDSFrontEnd::WriteFile(
    FileIdT FileId,
    BufferT SourceBuffer,
    FileSizeT Offset,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }

        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = false;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->BytesDesired = BytesToWrite;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = nullptr;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFile(
        FileId,
        Offset,
        SourceBuffer,
        BytesToWrite,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Async write to a file with gathering
// 
//
ErrorCodeT
DDSFrontEnd::WriteFileGather(
    FileIdT FileId,
    BufferT* SourceBufferArray,
    FileSizeT Offset,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollT* poll = AllPolls[AllFiles[FileId]->PollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    //
    // Lock-free busy polling for completion
    //
    //
    bool expectedCompletion = true;
    while (pIO->IsComplete.compare_exchange_weak(
        expectedCompletion,
        false,
        std::memory_order_relaxed
    ) == false) {
        if (pIO->AppCallback == nullptr) {
            return DDS_ERROR_CODE_REQUIRE_POLLING;
        }
    }

    pIO->IsRead = false;
    pIO->IsSegmented = true;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = SourceBufferArray;
    pIO->BytesDesired = BytesToWrite;
    pIO->BytesServiced = 0;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFileGather(
        pIO->FileId,
        pIO->Offset,
        pIO->AppBufferArray,
        pIO->BytesDesired,
        &pIO->BytesServiced,
        pIO,
        poll
    );

    if (BytesWritten) {
        *BytesWritten = pIO->BytesServiced;
    }

    if (!Callback) {
        return DDS_ERROR_CODE_IO_PENDING;
    }

    return result;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Async write to a file with gathering
// 
//
ErrorCodeT
DDSFrontEnd::WriteFileGather(
    FileIdT FileId,
    BufferT* SourceBufferArray,
    FileSizeT Offset,
    FileIOSizeT BytesToWrite,
    FileIOSizeT* BytesWritten,
    ReadWriteCallback Callback,
    ContextT Context
) {
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    PollIdT pollId = AllFiles[FileId]->PollId;
    PollT* poll = AllPolls[pollId];
    size_t mySlot = poll->NextRequestSlot.fetch_add(1, std::memory_order_relaxed);
    mySlot %= DDS_MAX_OUTSTANDING_IO;
    FileIOT* pIO = poll->OutstandingRequests[mySlot];

    bool expectedAvail = true;
    if (pIO->IsAvailable.compare_exchange_weak(
        expectedAvail,
        false,
        std::memory_order_relaxed
    ) == false) {
        //
        // If this is a callback-based completion,
        // perform polling once
        //
        //
        if (pIO->AppCallback) {
            FileIOSizeT bytesServiced;
            ContextT fileCtxt;
            ContextT ioCtxt;
            bool pollResult;

            PollWait(
                pollId,
                &bytesServiced,
                &fileCtxt,
                &ioCtxt,
                0,
                &pollResult
            );
        }
        return DDS_ERROR_CODE_TOO_MANY_REQUESTS;
    }

    pIO->IsRead = false;
    pIO->FileReference = AllFiles[FileId];
    pIO->FileId = FileId;
    pIO->Offset = Offset;
    pIO->BytesDesired = BytesToWrite;
    pIO->AppBuffer = nullptr;
    pIO->AppBufferArray = nullptr;
    pIO->AppCallback = Callback;
    pIO->Context = Context;

    result = BackEnd->WriteFileGather(
        FileId,
        Offset,
        SourceBufferArray,
        BytesToWrite,
        nullptr,
        pIO,
        poll
    );

    if (result != DDS_ERROR_CODE_IO_PENDING) {
        pIO->IsAvailable = true;
        return result;
    }

    //
    // Update file pointer
    //
    //
    ((DDSFile*)pIO->FileReference)->IncrementPointer(BytesToWrite);

    return DDS_ERROR_CODE_IO_PENDING;
}
#else
#error "Unknown backend type"
#endif

//
// Flush buffered data to storage
// Note: buffering is disabled
// 
//
ErrorCodeT
DDSFrontEnd::FlushFileBuffers(
    FileIdT FileId
) {
    return DDS_ERROR_CODE_SUCCESS;
}

//
// Search a directory for a file or subdirectory
// with a name that matches a specific name
// (or partial name if wildcards are used)
// 
//
ErrorCodeT
DDSFrontEnd::FindFirstFile(
    const char* FileName,
    FileIdT* FileId
) {
    //
    // TODO: support wild card search
    //
    //
    FileIdT id = DDS_FILE_INVALID;

    for (FileIdT i = 0; i != FileIdEnd; i++) {
        if (AllFiles[i]) {
            if (strcmp(FileName, AllFiles[i]->GetName()) == 0) {
                id = i;
                break;
            }
        }
    }

    if (id == DDS_FILE_INVALID) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    *FileId = id;

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Continue a file search from a previous call
// to the FindFirstFile
// 
//
ErrorCodeT
DDSFrontEnd::FindNextFile(
    FileIdT LastFileId,
    FileIdT* FileId
) {
    *FileId = DDS_FILE_INVALID;

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get file properties by file id
// 
//
ErrorCodeT
DDSFrontEnd::GetFileInformationById(
    FileIdT FileId,
    FilePropertiesT* FileProperties
) {
    DDSFile* pFile = AllFiles[FileId];
    FileProperties->Access = pFile->GetAccess();
    FileProperties->Position = pFile->GetPointer();
    FileProperties->ShareMode = pFile->GetShareMode();
    FileProperties->FileAttributes = pFile->GetAttributes();
    FileProperties->CreationTime = pFile->GetCreationTime();
    FileProperties->LastAccessTime = pFile->GetLastAccessTime();
    FileProperties->LastWriteTime = pFile->GetLastWriteTime();

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get file attributes by file name
// 
//
ErrorCodeT
DDSFrontEnd::GetFileAttributes(
    const char* FileName,
    FileAttributesT* FileAttributes
) {
    FileIdT id = DDS_FILE_INVALID;

    for (FileIdT i = 0; i != FileIdEnd; i++) {
        if (AllFiles[i] && strcmp(FileName, AllFiles[i]->GetName()) == 0) {
            id = i;
            break;
        }
    }

    if (id == DDS_FILE_INVALID) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    //
    // File attributes might be updated on the DPU, so get it from back end
    //
    //
    return BackEnd->GetFileAttributes(id, FileAttributes);
}

//
// Retrieve the current directory for the
// current process
// 
//
ErrorCodeT
DDSFrontEnd::GetCurrentDirectory(
    unsigned BufferLength,
    BufferT DirectoryBuffer
) {
    //
    // TODO: this function is more like an OS feature
    // check whether it is necessary
    //
    //
    DDSDir* pDir = AllDirs[DDS_DIR_ROOT];
    if (strlen(pDir->GetName()) > BufferLength) {
        return DDS_ERROR_CODE_BUFFER_OVERFLOW;
    }

    strcpy((char*)DirectoryBuffer, pDir->GetName());

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get the size of free space on the storage
// 
//
ErrorCodeT
DDSFrontEnd::GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace
) {
    return BackEnd->GetStorageFreeSpace(StorageFreeSpace);
}

//
// Lock the specified file for exclusive
// access by the calling process
// 
//
ErrorCodeT
DDSFrontEnd::LockFile(
    FileIdT FileId,
    PositionInFileT FileOffset,
    PositionInFileT NumberOfBytesToLock
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Unlock a region in an open file;
// Unlocking a region enables other
// processes to access the region
// 
//
ErrorCodeT
DDSFrontEnd::UnlockFile(
    FileIdT FileId,
    PositionInFileT FileOffset,
    PositionInFileT NumberOfBytesToUnlock
) {
    return DDS_ERROR_CODE_NOT_IMPLEMENTED;
}

//
// Move an existing file or a directory,
// including its children
// 
//
ErrorCodeT
DDSFrontEnd::MoveFile(
    const char* ExistingFileName,
    const char* NewFileName
) {
    FileIdT id = DDS_FILE_INVALID;
    DDSFile* pFile = NULL;

    for (FileIdT i = 0; i != FileIdEnd; i++) {
        if (AllFiles[i] && strcmp(ExistingFileName, AllFiles[i]->GetName()) == 0) {
            id = i;
            pFile = AllFiles[i];
            break;
        }
    }

    if (id == DDS_FILE_INVALID) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    pFile->SetName(NewFileName);

    //
    // Reflect the update on back end
    //
    //
    return BackEnd->MoveFile(id, NewFileName);
}

//
// Get the default poll structure for async I/O
//
//
ErrorCodeT
DDSFrontEnd::GetDefaultPoll(
    PollIdT* PollId
) {
    *PollId = DDS_POLL_DEFAULT;

    return DDS_ERROR_CODE_SUCCESS;
}


//
// Create a poll structure for async I/O
//
//
ErrorCodeT
DDSFrontEnd::PollCreate(
    PollIdT* PollId
) {
    //
    // Retrieve an id
    //
    //
    PollIdT id = 0;
    for (; id != DDS_MAX_POLLS; id++) {
        if (!AllPolls[id]) {
            break;
        }
    }

    if (id == DDS_MAX_POLLS) {
        return DDS_ERROR_CODE_TOO_MANY_POLLS;
    }

    PollT* poll = new PollT();
    for (size_t i = 0; i != DDS_MAX_OUTSTANDING_IO; i++) {
        poll->OutstandingRequests[i] = new FileIOT();
    }
    poll->NextRequestSlot = 0;
    AllPolls[id] = poll;

    *PollId = id;

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Delete a poll structure
//
//
ErrorCodeT
DDSFrontEnd::PollDelete(
    PollIdT PollId
) {
    if (PollId == DDS_POLL_DEFAULT) {
        return DDS_ERROR_CODE_INVALID_POLL_DELETION;
    }

    PollT* poll = AllPolls[PollId];
    if (!poll) {
        return DDS_ERROR_CODE_INVALID_POLL_DELETION;
    }

    for (size_t i = 0; i != DDS_MAX_OUTSTANDING_IO; i++) {
        delete poll->OutstandingRequests[i];
    }
    delete poll;
    AllPolls[PollId] = nullptr;

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Add a file to a pool
//
//
ErrorCodeT
DDSFrontEnd::PollAdd(
    FileIdT FileId,
    PollIdT PollId,
    ContextT FileContext
) {
    DDSFile* pFile = AllFiles[FileId];

    if (PollId != pFile->PollId) {
        pFile->PollId = PollId;
    }

    pFile->PollContext = FileContext;

    return DDS_ERROR_CODE_SUCCESS;
}

#if BACKEND_TYPE == BACKEND_TYPE_LOCAL_MEMORY
//
// Poll a completion event
//
//
ErrorCodeT
DDSFrontEnd::PollWait(
    PollIdT PollId,
    FileIOSizeT* BytesServiced,
    ContextT* FileContext,
    ContextT* IOContext,
    size_t WaitTime,
    bool* PollResult
) {
    std::chrono::time_point<std::chrono::system_clock> begin, cur;

    PollT* poll = AllPolls[PollId];

    *PollResult = false;

    begin = std::chrono::system_clock::now();

    size_t sleepTimeInUs = 1;

    while (true) {
        for (size_t i = 0; i != DDS_MAX_OUTSTANDING_IO; i++) {
            FileIOT* io = poll->OutstandingRequests[i];

            bool expectedCompletion = true;
            if (io->IsReadyForPoll.compare_exchange_weak(
                expectedCompletion,
                false,
                std::memory_order_relaxed
            )) {
                *BytesServiced = io->BytesServiced;
                *FileContext = AllFiles[io->FileId]->PollContext;
                *IOContext = io->Context;
                *PollResult = true;

                io->IsComplete = true;

                break;
            }
        }

        if (*PollResult) {
            break;
        }

        cur = std::chrono::system_clock::now();
        if ((size_t)std::chrono::duration_cast<std::chrono::milliseconds>(cur - begin).count() >= WaitTime) {
            break;
        }

        //
        // Adaptively yeild the CPU usage until the tolerated latency upper bound
        //
        //
        std::this_thread::sleep_for(std::chrono::microseconds(sleepTimeInUs));
        if (sleepTimeInUs < DDS_POLL_MAX_LATENCY_MICROSECONDS) {
            sleepTimeInUs *= 2;
        }
    }

    if (!(*PollResult)) {
        *FileContext = nullptr;
        *IOContext = nullptr;
    }

    return DDS_ERROR_CODE_SUCCESS;
}
#elif BACKEND_TYPE == BACKEND_TYPE_DPU
//
// Poll a completion event
//
//
ErrorCodeT
DDSFrontEnd::PollWait(
    PollIdT PollId,
    FileIOSizeT* BytesServiced,
    ContextT* FileContext,
    ContextT* IOContext,
    size_t WaitTime,
    bool* PollResult
) {
    std::chrono::time_point<std::chrono::system_clock> begin, cur;

    PollT* poll = AllPolls[PollId];
    RequestIdT reqId;

    ErrorCodeT result = BackEnd->GetResponse(
        poll,
        WaitTime,
        BytesServiced,
        &reqId
    );

    if (result == DDS_ERROR_CODE_NO_COMPLETION) {
        *PollResult = false;
        *FileContext = nullptr;
        *IOContext = nullptr;
    }
    else {
        FileIOT* io = poll->OutstandingRequests[reqId];
        *FileContext = AllFiles[io->FileId]->PollContext;
        *IOContext = io->Context;
        *PollResult = true;

        //
        // If the application provides a callback, we should handle it here
        // because this poll might be invoked by a read/write call
        //
        //
        if (io->AppCallback) {
            io->AppCallback(result, *BytesServiced, io->Context);
        }

        //
        // Mark this slot as available
        //
        //
        io->IsAvailable.store(true);
    }

    return DDS_ERROR_CODE_SUCCESS;
}
#else
#error "Unknown backend type"
#endif

}