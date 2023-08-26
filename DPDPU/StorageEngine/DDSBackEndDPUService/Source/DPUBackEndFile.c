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
    struct DPUFile* File
){
    return File->Properties.FProperties.FileName;
}

FileAttributesT GetAttributes(
    struct DPUFile* File
){
    return File->Properties.FProperties.FileAttributes;
}

FileSizeT GetSize(
    struct DPUFile* File
){
    return File->Properties.FProperties.FileSize;
}

SegmentSizeT GetFileAddressOnSegment(
    struct DPUFile* File
){
    return File->AddressOnSegment;
}

DPUFilePropertiesT* GetFileProperties(
    struct DPUFile* File
){
    return &File->Properties;
}

SegmentIdT GetNumSegments(
    struct DPUFile* File
){
    return File->NumSegments;
}

void SetName(
    const char* FileName,
    struct DPUFile* File
){
    strcpy(File->Properties.FProperties.FileName, FileName);
}

void SetSize(
    FileSizeT FileSize,
    struct DPUFile* File
){
    File->Properties.FProperties.FileSize = FileSize;
}

void SetNumAllocatedSegments(
    struct DPUFile* File
){
    File->NumSegments = 0;
    for (size_t s = 0; s != DDS_BACKEND_MAX_SEGMENTS_PER_FILE; s++) {
        if (File->Properties.Segments[s] == DDS_BACKEND_SEGMENT_INVALID) {
            break;
        }
        File->NumSegments++;
    }
}

inline void AllocateSegment(
    SegmentIdT NewSegment,
    struct DPUFile* File
){
    File->Properties.Segments[File->NumSegments] = NewSegment;
    File->NumSegments++;
}

inline void DeallocateSegment(
    struct DPUFile* File
){
    File->NumSegments--;
    File->Properties.Segments[File->NumSegments] = DDS_BACKEND_SEGMENT_INVALID;
}