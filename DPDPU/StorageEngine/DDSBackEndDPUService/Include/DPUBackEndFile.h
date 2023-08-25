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

const char* GetName();

FileAttributesT GetAttributes();

FileSizeT GetSize();

SegmentSizeT GetAddressOnSegment();

PersistentPropertiesT* GetProperties();

SegmentIdT GetNumSegments();

void SetName(
    const char* FileName
);

void SetSize(
    FileSizeT FileSize
);

//
// Set number of segments based on the allocation
//
//
void SetNumAllocatedSegments();

//
// Allocate a segment
// Assuming boundries were taken care of when the function is invoked
// Not thread-safe
//
//
inline void AllocateSegment(
    SegmentIdT NewSegment
);

//
// Deallocate a segment
// Assuming boundries were taken care of when the function is invoked
// Not thread-safe
//
//
inline void DeallocateSegment();