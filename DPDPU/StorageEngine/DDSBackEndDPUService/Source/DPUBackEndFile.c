#include <string.h>

#include "DPUBackEndFile.h"

struct DPUFile* BackEndFile(){
    struct DPUFile *tmp;
    tmp = malloc(sizeof(struct DPUFile));
    tmp->Properties.Id = DDS_FILE_INVALID;
    tmp->Properties.FProperties.FileAttributes = 0;
    tmp->Properties.FProperties.FileSize = 0;

    for (size_t s = 0; s != DDS_BACKEND_MAX_SEGMENTS_PER_FILE; s++) {
        tmp->Properties.Segments[s] = DDS_BACKEND_SEGMENT_INVALID;
    }

    tmp->NumSegments = 0;
    tmp->AddressOnSegment = 0;
    return tmp;
}

struct DPUFile* BackEndFile(
    FileIdT FileId,
    const char* FileName,
    FileAttributesT FileAttributes
){
    struct DPUFile *tmp;
    tmp = malloc(sizeof(struct DPUFile));
    tmp->Properties.Id = FileId;
    strcpy(tmp->Properties.FProperties.FileName, FileName);
    tmp->Properties.FProperties.FileAttributes = FileAttributes;
    tmp->Properties.FProperties.FileSize = 0;

    for (size_t s = 0; s != DDS_BACKEND_MAX_SEGMENTS_PER_FILE; s++) {
        tmp->Properties.Segments[s] = DDS_BACKEND_SEGMENT_INVALID;
    }

    tmp->NumSegments = 0;
    tmp->AddressOnSegment = DDS_BACKEND_SEGMENT_SIZE - FileId * sizeof(DPUFilePropertiesT);
    return tmp;
}

const char* GetName(
    struct DPUFile* Dir
){
    return Dir->Properties.FProperties.FileName;
}

FileAttributesT GetAttributes(
    struct DPUFile* Dir
){
    return Dir->Properties.FProperties.FileAttributes;
}

FileSizeT GetSize(
    struct DPUFile* Dir
){
    return Dir->Properties.FProperties.FileSize;
}

SegmentSizeT GetAddressOnSegment(
    struct DPUFile* Dir
){
    return Dir->AddressOnSegment;
}

DPUFilePropertiesT* GetProperties(
    struct DPUFile* Dir
){
    return &Dir->Properties;
}

SegmentIdT GetNumSegments(
    struct DPUFile* Dir
){
    return Dir->NumSegments;
}

void SetName(
    const char* FileName,
    struct DPUFile* Dir
){
    strcpy(Dir->Properties.FProperties.FileName, FileName);
}

void SetSize(
    FileSizeT FileSize,
    struct DPUFile* Dir
){
    Dir->Properties.FProperties.FileSize = FileSize;
}

void SetNumAllocatedSegments(
    struct DPUFile* Dir
){
    Dir->NumSegments = 0;
    for (size_t s = 0; s != DDS_BACKEND_MAX_SEGMENTS_PER_FILE; s++) {
        if (Dir->Properties.Segments[s] == DDS_BACKEND_SEGMENT_INVALID) {
            break;
        }
        Dir->NumSegments++;
    }
}

inline void AllocateSegment(
    SegmentIdT NewSegment,
    struct DPUFile* Dir
){
    Dir->Properties.Segments[Dir->NumSegments] = NewSegment;
    Dir->NumSegments++;
}

inline void DeallocateSegment(
    struct DPUFile* Dir
){
    Dir->NumSegments--;
    Dir->Properties.Segments[Dir->NumSegments] = DDS_BACKEND_SEGMENT_INVALID;
}