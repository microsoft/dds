#pragma once

#include "DPUBackEnd.h"
#include <stdbool.h>


//
// Persistent properties of DPU file
//
//
typedef struct DPUFileProperties {
        FileIdT Id;
        FilePropertiesT FProperties;
        //remove const before SegmentIdT
        SegmentIdT Segments[DDS_BACKEND_MAX_SEGMENTS_PER_FILE];
} DPUFilePropertiesT;

//
// DPU file, highly similar with DDSBackEndFile
//
//
typedef struct DPUFile {
    DPUFilePropertiesT Properties;
    SegmentIdT NumSegments;
    //remove const before SegmentIdT
    SegmentSizeT AddressOnSegment;
};

//
// BackEndFile constructor without input
//
//
struct DPUFile* BackEndFileX();

//
// BackEndFile constructor with input
//
//
struct DPUFile* BackEndFileI(
    FileIdT FileId,
    const char* FileName,
    FileAttributesT FileAttributes
);

const char* GetName(struct DPUFile* File);

FileAttributesT GetAttributes(struct DPUFile* File);

FileSizeT GetSize(struct DPUFile* File);

SegmentSizeT GetFileAddressOnSegment(struct DPUFile* File);

DPUFilePropertiesT* GetFileProperties(struct DPUFile* File);

SegmentIdT GetNumSegments(struct DPUFile* File);

void SetName(
    const char* FileName,
    struct DPUFile* File
);

void SetSize(
    FileSizeT FileSize,
    struct DPUFile* File
);

//
// Set number of segments based on the allocation
//
//
void SetNumAllocatedSegments(struct DPUFile* File);

//
// Allocate a segment
// Assuming boundries were taken care of when the function is invoked
// Not thread-safe
//
//
inline void AllocateSegment(
    SegmentIdT NewSegment,
    struct DPUFile* File
);

//
// Deallocate a segment
// Assuming boundries were taken care of when the function is invoked
// Not thread-safe
//
//
inline void DeallocateSegment(struct DPUFile* File);