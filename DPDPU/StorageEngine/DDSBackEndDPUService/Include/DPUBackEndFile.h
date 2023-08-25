#pragma once

#include "DPUBackEnd.h"


//
// Persistent properties of DPU file
//
//
typedef struct DPUFileProperties {
        FileIdT Id;
        FileProperties FProperties;
        SegmentIdT Segments[DDS_BACKEND_MAX_SEGMENTS_PER_FILE];
} DPUFilePropertiesT;

//
// DPU file, highly similar with DDSBackEndFile
//
//
typedef struct DPUFile {
    DPUFilePropertiesT Properties;
    SegmentIdT NumSegments;
    const SegmentSizeT AddressOnSegment;
};

DPUFile BackEndFile();

DPUFile BackEndFile(
    FileIdT FileId,
    const char* FileName,
    FileAttributesT FileAttributes
);

const char* GetName(struct DPUFile* Dir);

FileAttributesT GetAttributes(struct DPUFile* Dir);

FileSizeT GetSize(struct DPUFile* Dir);

SegmentSizeT GetAddressOnSegment(struct DPUFile* Dir);

DPUFilePropertiesT* GetProperties(struct DPUFile* Dir);

SegmentIdT GetNumSegments(struct DPUFile* Dir);

void SetName(
    const char* FileName,
    struct DPUFile* Dir
);

void SetSize(
    FileSizeT FileSize,
    struct DPUFile* Dir
);

//
// Set number of segments based on the allocation
//
//
void SetNumAllocatedSegments(struct DPUFile* Dir);

//
// Allocate a segment
// Assuming boundries were taken care of when the function is invoked
// Not thread-safe
//
//
inline void AllocateSegment(
    SegmentIdT NewSegment,
    struct DPUFile* Dir
);

//
// Deallocate a segment
// Assuming boundries were taken care of when the function is invoked
// Not thread-safe
//
//
inline void DeallocateSegment(struct DPUFile* Dir);