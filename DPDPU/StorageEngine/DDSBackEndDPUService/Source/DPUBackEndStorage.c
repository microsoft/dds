//#include <iostream>
// #include <threads.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>

#include "../Include/DPUBackEndStorage.h"

//
// Constructor
// 
//
struct DPUStorage* BackEndStorage(){
    struct DPUStorage *tmp;
    tmp = malloc(sizeof(struct DPUStorage));
    tmp->AllSegments = NULL;
    tmp->AvailableSegments = 0;

    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        tmp->AllDirs[d] = NULL;
    }

    for (size_t f = 0; f != DDS_MAX_FILES; f++) {
        tmp->AllFiles[f] = NULL;
    }

    tmp->TotalDirs = 0;
    tmp->TotalFiles = 0;

    tmp->TargetProgress = 0;
    tmp->CurrentProgress = 0;
    tmp->TotalSegments = DDS_BACKEND_CAPACITY / DDS_BACKEND_SEGMENT_SIZE;
    pthread_mutex_init(&tmp->SectorModificationMutex, NULL);
    pthread_mutex_init(&tmp->SegmentAllocationMutex, NULL);
    return tmp;
}

//
// Destructor
// 
//
void DeBackEndStorage(
    struct DPUStorage* Sto
){
    //
    // Free all bufffers
    //
    //
    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        if (Sto->AllDirs[d] != NULL) {
            free(Sto->AllDirs[d]);
        }
    }

    for (size_t f = 0; f != DDS_MAX_FILES; f++) {
        if (Sto->AllFiles[f] != NULL) {
            free(Sto->AllFiles[f]);
        }
    }

    //
    // Return all segments
    //
    //
    ReturnSegments(Sto);
}

//
// Read from disk synchronously
//
//
ErrorCodeT ReadFromDiskSync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    struct DPUStorage* Sto
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    memcpy(DstBuffer, (char*)seg->DiskAddress + SegmentOffset, Bytes);
    
    return DDS_ERROR_CODE_SUCCESS;
}

//
// Write from disk synchronously
//
//
ErrorCodeT WriteToDiskSync(
    BufferT SrcBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    struct DPUStorage* Sto
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    memcpy((char*)seg->DiskAddress + SegmentOffset, SrcBuffer, Bytes);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Read from disk asynchronously
//
//
ErrorCodeT ReadFromDiskAsync(
    BufferT DstBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    memcpy(DstBuffer, (char*)seg->DiskAddress + SegmentOffset, Bytes);

    Callback(true, Context);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Write from disk asynchronously
//
//
ErrorCodeT WriteToDiskAsync(
    BufferT SrcBuffer,
    SegmentIdT SegmentId,
    SegmentSizeT SegmentOffset,
    FileIOSizeT Bytes,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto
){
    SegmentT* seg = &Sto->AllSegments[SegmentId];
    memcpy((char*)seg->DiskAddress + SegmentOffset, SrcBuffer, Bytes);

    Callback(true, Context);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Retrieve all segments on the disk, replace all new... by malloc() inside
//
//
ErrorCodeT RetrieveSegments(
    struct DPUStorage* Sto
){
    //
    // We should get data from the disk,
    // but just create all segments using memory for now
    //
    //
    Sto->AllSegments = malloc(sizeof(SegmentT) * Sto->TotalSegments);

    for (SegmentIdT i = 0; i != Sto->TotalSegments; i++) {
        Sto->AllSegments[i].Id = i;
        Sto->AllSegments[i].FileId = DDS_FILE_INVALID;
        
        char* newSegment = malloc(sizeof(char) * DDS_BACKEND_SEGMENT_SIZE);

        if (!newSegment) {
            return DDS_ERROR_CODE_SEGMENT_RETRIEVAL_FAILURE;
        }

        memset(newSegment, 0, sizeof(char) * DDS_BACKEND_SEGMENT_SIZE);
        Sto->AllSegments[i].DiskAddress = (DiskSizeT)newSegment;
        
        if (i == DDS_BACKEND_RESERVED_SEGMENT) {
            Sto->AllSegments[i].Allocatable = false;
        }
        else {
            Sto->AllSegments[i].Allocatable = true;
            Sto->AvailableSegments++;
        }
    }

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Return all segments back to the disk
//
//
void ReturnSegments(
    struct DPUStorage* Sto
){
    //
    // Release the memory buffers
    //
    //
    if (Sto->AllSegments) {
        for (size_t i = 0; i != Sto->TotalSegments; i++) {
            if (Sto->AllSegments[i].DiskAddress) {
                //delete[] (char*)AllSegments[i].DiskAddress;
                //not sure is this correct?
                free((char*)Sto->AllSegments[i].DiskAddress);
            }

            if (Sto->AllSegments[i].Allocatable) {
                Sto->AvailableSegments--;
            }
        }

        //delete[] AllSegments;
        free(Sto->AllSegments);
    }
}

//
// Load all directories and files from the reserved segment
//
//
ErrorCodeT LoadDirectoriesAndFiles(
    struct DPUStorage* Sto
){
    if (!Sto->AllSegments || Sto->TotalSegments == 0) {
        return DDS_ERROR_CODE_RESERVED_SEGMENT_ERROR;
    }

    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;

    //
    // Read the first sector
    //
    //
    char tmpSectorBuffer[DDS_BACKEND_SECTOR_SIZE];
    result = ReadFromDiskSync(
        tmpSectorBuffer,
        DDS_BACKEND_RESERVED_SEGMENT,
        0,
        DDS_BACKEND_SECTOR_SIZE,
        Sto
    );

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }
    Sto->TotalDirs = *((int*)(tmpSectorBuffer + DDS_BACKEND_INITIALIZATION_MARK_LENGTH));
    Sto->TotalFiles = *((int*)(tmpSectorBuffer + DDS_BACKEND_INITIALIZATION_MARK_LENGTH + sizeof(int)));

    //
    // Load the directories
    //
    //
    DPUDirPropertiesT dirOnDisk;
    SegmentSizeT nextAddress = DDS_BACKEND_SECTOR_SIZE;
    int loadedDirs = 0;
    for (size_t d = 0; d != DDS_MAX_DIRS; d++) {
        result = ReadFromDiskSync(
            (BufferT)&dirOnDisk,
            DDS_BACKEND_RESERVED_SEGMENT,
            nextAddress,
            sizeof(DPUDirPropertiesT),
            Sto
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return result;
        }

        if (dirOnDisk.Id != DDS_DIR_INVALID) {
            Sto->AllDirs[d] = BackEndDirI(dirOnDisk.Id, DDS_DIR_ROOT, dirOnDisk.Name);
            if (Sto->AllDirs[d]) {
                //why do we have 2 sizeof() here
                memcpy(GetDirProperties(Sto->AllDirs[d]), &dirOnDisk, sizeof(sizeof(DPUDirPropertiesT)));
            }
            else {
                return DDS_ERROR_CODE_OUT_OF_MEMORY;
            }

            loadedDirs++;
            if (loadedDirs == Sto->TotalDirs) {
                break;
            }
        }
        
        nextAddress += sizeof(DPUDirPropertiesT);
    }
    //
    // Load the files
    //
    //
    DPUFilePropertiesT fileOnDisk;
    nextAddress = DDS_BACKEND_SEGMENT_SIZE - sizeof(DPUFilePropertiesT);
    int loadedFiles = 0;
    for (size_t f = 0; f != DDS_MAX_FILES; f++) {
        result = ReadFromDiskSync(
            (BufferT)&fileOnDisk,
            DDS_BACKEND_RESERVED_SEGMENT,
            nextAddress,
            sizeof(DPUFilePropertiesT),
            Sto
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return result;
        }

        if (fileOnDisk.Id != DDS_DIR_INVALID) {
            Sto->AllFiles[f] = BackEndFileI(fileOnDisk.Id, fileOnDisk.FProperties.FileName, fileOnDisk.FProperties.FileAttributes);
            if (Sto->AllFiles[f]) {
                memcpy(GetFileProperties(Sto->AllFiles[f]), &fileOnDisk, sizeof(sizeof(DPUFilePropertiesT)));
                SetNumAllocatedSegments(Sto->AllFiles[f]);
            }
            else {
                return DDS_ERROR_CODE_OUT_OF_MEMORY;
            }

            loadedFiles++;
            if (loadedFiles == Sto->TotalFiles) {
                break;
            }
        }
        
        nextAddress -= sizeof(DPUFilePropertiesT);
    }

    return result;
}

//
// Synchronize a directory to the disk
//
//
inline ErrorCodeT SyncDirToDisk(
    struct DPUDir* Dir, 
    struct DPUStorage* Sto
){
    return WriteToDiskSync(
        (BufferT)GetDirProperties(Dir),
        DDS_BACKEND_RESERVED_SEGMENT,
        GetDirAddressOnSegment(Dir),
        sizeof(DPUDirPropertiesT),
        Sto
    );
}

//
// Synchronize a file to the disk
//
//
inline ErrorCodeT SyncFileToDisk(
    struct DPUFile* File,
    struct DPUStorage* Sto
){
    return WriteToDiskSync(
        (BufferT)GetFileProperties(File),
        DDS_BACKEND_RESERVED_SEGMENT,
        GetFileAddressOnSegment(File),
        sizeof(DPUFilePropertiesT),
        Sto
    );
}

//
// Synchronize the first sector on the reserved segment
//
//
inline ErrorCodeT SyncReservedInformationToDisk(
    struct DPUStorage* Sto
){
    char tmpSectorBuf[DDS_BACKEND_SECTOR_SIZE];

    memset(tmpSectorBuf, 0, DDS_BACKEND_SECTOR_SIZE);
    strcpy(tmpSectorBuf, DDS_BACKEND_INITIALIZATION_MARK);

    int* numDirs = (int*)(tmpSectorBuf + DDS_BACKEND_INITIALIZATION_MARK_LENGTH);
    *numDirs = Sto->TotalDirs;
    int* numFiles = numDirs + 1;
    *numFiles = Sto->TotalFiles;

    return WriteToDiskSync(
        tmpSectorBuf,
        DDS_BACKEND_RESERVED_SEGMENT,
        0,
        DDS_BACKEND_SECTOR_SIZE,
        Sto
    );
}


//
// Increment progress callback
//
//
void
IncrementProgressCallback(
    bool Success,
    ContextT Context) {
    atomic_size_t* progress = (atomic_size_t*)Context;
    (*progress) += 1;
}

//
// Initialize the backend service
//
//
ErrorCodeT Initialize(
    struct DPUStorage* Sto
){
    //
    // Retrieve all segments on disk
    //
    //
    ErrorCodeT result = RetrieveSegments(Sto);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    //
    // The first sector on the reserved segement contains initialization information
    //
    //
    bool diskInitialized = false;
    SegmentT* rSeg = &Sto->AllSegments[DDS_BACKEND_RESERVED_SEGMENT];
    char tmpSectorBuf[DDS_BACKEND_SECTOR_SIZE];
    memset(tmpSectorBuf, 0, DDS_BACKEND_SECTOR_SIZE);

    result = ReadFromDiskSync(tmpSectorBuf, DDS_BACKEND_RESERVED_SEGMENT, 0, DDS_BACKEND_SECTOR_SIZE, Sto);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }
    if (strcmp(DDS_BACKEND_INITIALIZATION_MARK, tmpSectorBuf)) {
        //
        // Empty every byte on the reserved segment
        //
        //
        char tmpPageBuf[DDS_BACKEND_PAGE_SIZE];
        memset(tmpPageBuf, 0, DDS_BACKEND_SECTOR_SIZE);
        
        size_t pagesPerSegment = DDS_BACKEND_SEGMENT_SIZE / DDS_BACKEND_PAGE_SIZE;
        size_t numPagesWritten = 0;

        //
        // Issue writes with the maximum queue depth
        //
        //
        while ((numPagesWritten + DDS_BACKEND_QUEUE_DEPTH_PAGE_IO_DEFAULT) <= pagesPerSegment) {
            Sto->TargetProgress = pagesPerSegment;
            Sto->CurrentProgress = 0;
            for (size_t q = 0; q != DDS_BACKEND_QUEUE_DEPTH_PAGE_IO_DEFAULT; q++) {
                result = WriteToDiskAsync(
                    tmpPageBuf,
                    0,
                    (SegmentSizeT)((numPagesWritten + q) * DDS_BACKEND_PAGE_SIZE),
                    DDS_BACKEND_PAGE_SIZE,
                    IncrementProgressCallback,
                    &Sto->CurrentProgress,
                    Sto
                );

                if (result != DDS_ERROR_CODE_SUCCESS) {
                    return result;
                }
            }

            while (Sto->CurrentProgress != Sto->TargetProgress) {
                //std::this_thread::yield();
                //not sure does this function have the same effect
                //pthread_yield() is non-standard function so I replace it by
                sched_yield();
            }

            numPagesWritten += DDS_BACKEND_QUEUE_DEPTH_PAGE_IO_DEFAULT;
        }

        //
        // Handle the remaining pages
        //
        //
        size_t pagesLeft = pagesPerSegment - numPagesWritten;
        Sto->TargetProgress = pagesLeft;
        Sto->CurrentProgress = 0;
        for (size_t q = 0; q != pagesLeft; q++) {
            result = WriteToDiskAsync(
                tmpPageBuf,
                0,
                (SegmentSizeT)((numPagesWritten + q) * DDS_BACKEND_PAGE_SIZE),
                DDS_BACKEND_PAGE_SIZE,
                IncrementProgressCallback,
                &Sto->CurrentProgress,
                Sto
            );

            if (result != DDS_ERROR_CODE_SUCCESS) {
                return result;
            }
        }

        while (Sto->CurrentProgress != Sto->TargetProgress) {
            //std::this_thread::yield();
            sched_yield();
        }

        //
        // Create the root directory, which is on the second sector on the segment
        //
        //
        struct DPUDir* rootDir = BackEndDirI(DDS_DIR_ROOT, DDS_DIR_INVALID, DDS_BACKEND_ROOT_DIR_NAME);
        result = SyncDirToDisk(rootDir, Sto);

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return result;
        }

        Sto->AllDirs[DDS_DIR_ROOT] = rootDir;

        //
        // Set the formatted mark and the numbers of dirs and files
        //
        //
        Sto->TotalDirs = 1;
        Sto->TotalFiles = 0;
        result = SyncReservedInformationToDisk(Sto);

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return result;
        }
    }
    else {
        //
        // Load directories and files
        //
        //
        result = LoadDirectoriesAndFiles(Sto);

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return result;
        }

        //
        // Update segment information
        //
        //
        int checkedFiles = 0;
        for (FileIdT f = 0; f != DDS_MAX_FILES; f++) {
            struct DPUFile* file = Sto->AllFiles[f];
            if (file) {
                SegmentIdT* segments = GetFileProperties(file)->Segments;
                for (size_t s = 0; s != DDS_BACKEND_MAX_SEGMENTS_PER_FILE; s++) {
                    SegmentIdT currentSeg = segments[s];
                    if (currentSeg == DDS_BACKEND_SEGMENT_INVALID) {
                        break;
                    }
                    else {
                        Sto->AllSegments[currentSeg].FileId = f;
                    }
                }

                checkedFiles++;
                if (checkedFiles == Sto->TotalFiles) {
                    break;
                }
            }
        }
    }
}
//
// Create a diretory
// Assuming id and parent id have been computed by host
// 
//
ErrorCodeT CreateDirectory(
    const char* PathName,
    DirIdT DirId,
    DirIdT ParentId,
    struct DPUStorage* Sto
){
    struct DPUDir* dir = BackEndDirI(DirId, ParentId, PathName);
    if (!dir) {
        return DDS_ERROR_CODE_OUT_OF_MEMORY;
    }
    
    ErrorCodeT result = SyncDirToDisk(dir, Sto);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    Sto->AllDirs[DirId] = dir;

    //SectorModificationMutex.lock();
    pthread_mutex_lock(&Sto->SectorModificationMutex);

    Sto->TotalDirs++;
    result = SyncReservedInformationToDisk(Sto);

    //SectorModificationMutex.unlock();
    pthread_mutex_unlock(&Sto->SectorModificationMutex);

    return result;
}

//
// Delete a directory
// 
//
ErrorCodeT RemoveDirectory(
    DirIdT DirId,
    struct DPUStorage* Sto
){
    if (!Sto->AllDirs[DirId]) {
        return DDS_ERROR_CODE_SUCCESS;
    }

    GetDirProperties(Sto->AllDirs[DirId])->Id = DDS_DIR_INVALID;
    ErrorCodeT result = SyncDirToDisk(Sto->AllDirs[DirId], Sto);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    //delete AllDirs[DirId];
    free(Sto->AllDirs[DirId]);
    Sto->AllDirs[DirId] = NULL;

    //SectorModificationMutex.lock();
    pthread_mutex_lock(&Sto->SectorModificationMutex);

    Sto->TotalDirs--;
    result = SyncReservedInformationToDisk(Sto);

    //SectorModificationMutex.unlock();
    pthread_mutex_unlock(&Sto->SectorModificationMutex);

    return result;
}

//
// Create a file
// 
//
ErrorCodeT CreateFile(
    const char* FileName,
    FileAttributesT FileAttributes,
    FileIdT FileId,
    DirIdT DirId,
    struct DPUStorage* Sto
){
    struct DPUDir* dir = Sto->AllDirs[DirId];
    if (!dir) {
        return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }

    struct DPUFile* file = BackEndFileI(FileId, FileName, FileAttributes);
    if (!file) {
        return DDS_ERROR_CODE_OUT_OF_MEMORY;
    }

    //
    // Make file persistent
    //
    //
    ErrorCodeT result = SyncFileToDisk(file, Sto);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    //
    // Add this file to its directory and make the update persistent
    //
    //
    Lock(dir);
    
    AddFile(FileId, dir);
    result = SyncDirToDisk(dir, Sto);
    
    Unlock(dir);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    //
    // Finally, file is added
    //
    //
    Sto->AllFiles[DirId] = file;

    //SectorModificationMutex.lock();
    pthread_mutex_lock(&Sto->SectorModificationMutex);

    Sto->TotalFiles++;
    result = SyncReservedInformationToDisk(Sto);

    //SectorModificationMutex.unlock();
    pthread_mutex_unlock(&Sto->SectorModificationMutex);

    return result;
}

//
// Delete a file
// 
//
ErrorCodeT DeleteFileOnSto(
    FileIdT FileId,
    DirIdT DirId,
    struct DPUStorage* Sto
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    struct DPUDir* dir = Sto->AllDirs[DirId];

    if (!file || !dir) {
        return DDS_ERROR_CODE_SUCCESS;
    }

    GetFileProperties(file)->Id = DDS_FILE_INVALID;
    
    //
    // Make the change persistent
    //
    //
    ErrorCodeT result = SyncFileToDisk(file, Sto);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    //
    // Delete this file from its directory and make the update persistent
    //
    //
    Lock(dir);

    DeleteFile(FileId, dir);
    result = SyncDirToDisk(dir, Sto);

    Unlock(dir);

    //
    // Finally, delete the file
    //
    //
    free(file);
    Sto->AllFiles[FileId] = NULL;

    //SectorModificationMutex.lock();
    pthread_mutex_lock(&Sto->SectorModificationMutex);

    Sto->TotalFiles--;
    result = SyncReservedInformationToDisk(Sto);

   //SectorModificationMutex.unlock();
    pthread_mutex_unlock(&Sto->SectorModificationMutex);

    return result;
}

//
// Change the size of a file
// 
//
ErrorCodeT ChangeFileSize(
    FileIdT FileId,
    FileSizeT NewSize,
    struct DPUStorage* Sto
){
    struct DPUFile* file = Sto->AllFiles[FileId];

    if (!file) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    //
    // There might be concurrent writes, hoping apps will handle the concurrency
    //
    //
    FileSizeT currentFileSize = GetFileProperties(file)->FProperties.FileSize;
    long long sizeToChange = NewSize - currentFileSize;
    if (sizeToChange >= 0) {
        //
        // The previous code was "(size_t)GetNumSegments(file)", but it cannot be
        // understood by compiler, so I rewrite in this format
        // 
        //
        size_t numSeg = GetNumSegments(file); 
        SegmentSizeT remainingAllocatedSize = (SegmentSizeT)(numSeg * DDS_BACKEND_SEGMENT_SIZE - currentFileSize);
        long long bytesToBeAllocated = sizeToChange - (long long)remainingAllocatedSize;
        
        if (bytesToBeAllocated > 0) {
            SegmentIdT numSegmentsToAllocate = (SegmentIdT)(
                bytesToBeAllocated / DDS_BACKEND_SEGMENT_SIZE +
                (bytesToBeAllocated % DDS_BACKEND_SEGMENT_SIZE == 0 ? 0 : 1)
            );

            //
            // Allocate segments
            //
            //
            //SegmentAllocationMutex.lock();
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            if (numSegmentsToAllocate > Sto->AvailableSegments) {
                //SegmentAllocationMutex.unlock();
                pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
                return DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
            }

            SegmentIdT allocatedSegments = 0;
            for (SegmentIdT s = 0; s != Sto->TotalSegments; s++) {
                SegmentT* seg = &Sto->AllSegments[s];
                if (seg->Allocatable && seg->FileId == DDS_FILE_INVALID) {
                    AllocateSegment(s,file);
                    seg->FileId = FileId;
                    allocatedSegments++;
                    if (allocatedSegments == numSegmentsToAllocate) {
                        break;
                    }
                }
            }

            //SegmentAllocationMutex.unlock();
            pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
        }
    }
    else {
        SegmentIdT newSegments = (SegmentIdT)(NewSize / DDS_BACKEND_SEGMENT_SIZE + (NewSize % DDS_BACKEND_SEGMENT_SIZE == 0 ? 0 : 1));
        SegmentIdT numSegmentsToDeallocate = GetNumSegments(file) - newSegments;
        if (numSegmentsToDeallocate > 0) {
            //
            // Deallocate segments
            //
            //
            //SegmentAllocationMutex.lock();
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            for (SegmentIdT s = 0; s != numSegmentsToDeallocate; s++) {
                DeallocateSegment(file);
            }

            //SegmentAllocationMutex.unlock();
            pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
        }
    }

    //
    // Segment (de)allocation has been taken care of
    //
    //
    SetSize(NewSize, file);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get file size
// 
//
ErrorCodeT GetFileSize(
    FileIdT FileId,
    FileSizeT* FileSize,
    struct DPUStorage* Sto
){
    struct DPUFile* file = Sto->AllFiles[FileId];

    if (!file) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    *FileSize = GetSize(file);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Async read from a file
// 
//
ErrorCodeT ReadFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT DestBuffer,
    FileIOSizeT BytesToRead,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto
){
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;
    struct DPUFile* file = Sto->AllFiles[FileId];
    BackEndIOContextT* ioContext = (BackEndIOContextT*)Context;
    FileSizeT remainingBytes = GetSize(file) - Offset;

    FileIOSizeT bytesLeftToRead = BytesToRead;
    if (remainingBytes < BytesToRead) {
        bytesLeftToRead = (FileIOSizeT)remainingBytes;
    }

    ioContext->BytesIssued = bytesLeftToRead;

    FileSizeT curOffset = Offset;
    SegmentIdT curSegment;
    SegmentSizeT offsetOnSegment;
    SegmentSizeT bytesToIssue;
    SegmentSizeT remainingBytesOnCurSeg;
    while (bytesLeftToRead) {
        //
        // Cross-boundary detection
        //
        //
        curSegment = (SegmentIdT)(curOffset / DDS_BACKEND_SEGMENT_SIZE);
        offsetOnSegment = curOffset % DDS_BACKEND_SEGMENT_SIZE;
        remainingBytesOnCurSeg = DDS_BACKEND_SEGMENT_SIZE - offsetOnSegment;
        
        if (remainingBytesOnCurSeg >= bytesLeftToRead) {
            bytesToIssue = bytesLeftToRead;
        }
        else {
            bytesToIssue = remainingBytesOnCurSeg;
        }

        result = ReadFromDiskAsync(
            DestBuffer + (curOffset - Offset),
            curSegment,
            offsetOnSegment,
            bytesToIssue,
            Callback,
            Context,
            Sto
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return result;
        }

        curOffset += bytesToIssue;
        bytesLeftToRead -= bytesToIssue;
        
    }

    return result;
}


//
// Async write to a file
// 
//
ErrorCodeT WriteFile(
    FileIdT FileId,
    FileSizeT Offset,
    BufferT SourceBuffer,
    FileIOSizeT BytesToWrite,
    DiskIOCallback Callback,
    ContextT Context,
    struct DPUStorage* Sto
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    FileSizeT newSize = Offset + BytesToWrite;
    ErrorCodeT result = DDS_ERROR_CODE_SUCCESS;
    BackEndIOContextT* ioContext = (BackEndIOContextT*)Context;

    //
    // Check if allocation is needed
    //
    //
    FileSizeT currentFileSize = GetFileProperties(file)->FProperties.FileSize;
    long long sizeToChange = newSize - currentFileSize;

    if (sizeToChange >= 0) {
        size_t numSeg = GetNumSegments(file);
        SegmentSizeT remainingAllocatedSize = (SegmentSizeT)(numSeg * DDS_BACKEND_SEGMENT_SIZE - currentFileSize);
        long long bytesToBeAllocated = sizeToChange - (long long)remainingAllocatedSize;

        if (bytesToBeAllocated > 0) {
            SegmentIdT numSegmentsToAllocate = (SegmentIdT)(
                bytesToBeAllocated / DDS_BACKEND_SEGMENT_SIZE +
                (bytesToBeAllocated % DDS_BACKEND_SEGMENT_SIZE == 0 ? 0 : 1)
                );

            //
            // Allocate segments
            //
            //
            //SegmentAllocationMutex.lock();
            pthread_mutex_lock(&Sto->SegmentAllocationMutex);

            if (numSegmentsToAllocate > Sto->AvailableSegments) {
                //SegmentAllocationMutex.unlock();
                pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
                return DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE;
            }

            SegmentIdT allocatedSegments = 0;
            for (SegmentIdT s = 0; s != Sto->TotalSegments; s++) {
                SegmentT* seg = &Sto->AllSegments[s];
                if (seg->Allocatable && seg->FileId == DDS_FILE_INVALID) {
                    AllocateSegment(s,file);
                    seg->FileId = FileId;
                    allocatedSegments++;
                    if (allocatedSegments == numSegmentsToAllocate) {
                        break;
                    }
                }
            }

            //SegmentAllocationMutex.unlock();
                pthread_mutex_unlock(&Sto->SegmentAllocationMutex);
        }

        SetSize(newSize,file);
    }

    FileIOSizeT bytesLeftToWrite = BytesToWrite;
    ioContext->BytesIssued = bytesLeftToWrite;

    FileSizeT curOffset = Offset;
    SegmentIdT curSegment;
    SegmentSizeT offsetOnSegment;
    SegmentSizeT bytesToIssue;
    SegmentSizeT remainingBytesOnCurSeg;
    while (bytesLeftToWrite) {
        //
        // Cross-boundary detection
        //
        //
        curSegment = (SegmentIdT)(curOffset / DDS_BACKEND_SEGMENT_SIZE);
        offsetOnSegment = curOffset % DDS_BACKEND_SEGMENT_SIZE;
        remainingBytesOnCurSeg = DDS_BACKEND_SEGMENT_SIZE - offsetOnSegment;

        if (remainingBytesOnCurSeg >= bytesLeftToWrite) {
            bytesToIssue = bytesLeftToWrite;
        }
        else {
            bytesToIssue = remainingBytesOnCurSeg;
        }

        result = WriteToDiskAsync(
            SourceBuffer + (curOffset - Offset),
            curSegment,
            offsetOnSegment,
            bytesToIssue,
            Callback,
            Context,
            Sto
        );

        if (result != DDS_ERROR_CODE_SUCCESS) {
            return result;
        }

        curOffset += bytesToIssue;
        bytesLeftToWrite -= bytesToIssue;
    }

    return result;
}

//
// Get file properties by file Id
// 
//
ErrorCodeT GetFileInformationById(
    FileIdT FileId,
    FilePropertiesT* FileProperties,
    struct DPUStorage* Sto
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    if (file) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    FileProperties->FileAttributes = GetAttributes(file);
    strcpy(FileProperties->FileName, GetName(file));
    FileProperties->FileSize = GetSize(file);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get file attributes by file name
// 
//
ErrorCodeT
GetFileAttributes(
    FileIdT FileId,
    FileAttributesT* FileAttributes,
    struct DPUStorage* Sto
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    if (file) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }

    *FileAttributes = GetAttributes(file);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Get the size of free space on the storage
// 
//
ErrorCodeT GetStorageFreeSpace(
    FileSizeT* StorageFreeSpace,
    struct DPUStorage* Sto
){
    //SegmentAllocationMutex.lock();
    pthread_mutex_lock(&Sto->SegmentAllocationMutex);

    *StorageFreeSpace = Sto->AvailableSegments * (FileSizeT)DDS_BACKEND_SEGMENT_SIZE;

    //SegmentAllocationMutex.unlock();
    pthread_mutex_unlock(&Sto->SegmentAllocationMutex);

    return DDS_ERROR_CODE_SUCCESS;
}

//
// Move an existing file or a directory,
// including its children
// 
//
ErrorCodeT MoveFile(
    FileIdT FileId,
    DirIdT OldDirId,
    DirIdT NewDirId,
    const char* NewFileName,
    struct DPUStorage* Sto
){
    struct DPUFile* file = Sto->AllFiles[FileId];
    struct DPUDir* oldDir = Sto->AllDirs[OldDirId];
    struct DPUDir* newDir = Sto->AllDirs[NewDirId];
    if (!file) {
        return DDS_ERROR_CODE_FILE_NOT_FOUND;
    }
    if (!oldDir || !newDir) {
        return DDS_ERROR_CODE_DIR_NOT_FOUND;
    }

    //
    // Update old directory
    //
    //
    Lock(oldDir);

    ErrorCodeT result = DeleteFile(FileId, oldDir);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        Unlock(oldDir);
        return result;
    }

    result = SyncDirToDisk(oldDir, Sto);

    Unlock(oldDir);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    //
    // Update new directory
    //
    //
    Lock(newDir);
    
    result = AddFile(FileId, newDir);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        Unlock(newDir);
        return result;
    }

    result = SyncDirToDisk(newDir, Sto);

    Unlock(newDir);

    if (result != DDS_ERROR_CODE_SUCCESS) {
        return result;
    }

    SetName(NewFileName, file);
    
    return DDS_ERROR_CODE_SUCCESS;
}