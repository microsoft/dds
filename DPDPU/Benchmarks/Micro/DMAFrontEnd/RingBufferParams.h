#pragma once

#define DDS_PAGE_SIZE 4096
#define DDS_REQUEST_RING_SIZE 128
#define DDS_REQUEST_RING_SLOT_SIZE DDS_PAGE_SIZE
#define DDS_CACHE_LINE_SIZE 64
#define DDS_CACHE_LINE_SIZE_BY_INT 16
#define DDS_REQUEST_RING_BYTES 524288

#define RING_BUFFER_ALLOWABLE_TAIL_ADVANCEMENT 1048576

//
// Checking a few parameters at the compile time
//
//
#define assert_static(e, num) \
    enum { assert_static__##num = 1/(e) }

#pragma warning(push)
#pragma warning (disable: 4804)
assert_static(DDS_REQUEST_RING_BYTES == DDS_REQUEST_RING_SIZE * DDS_REQUEST_RING_SLOT_SIZE, 0);
assert_static(DDS_REQUEST_RING_BYTES% DDS_CACHE_LINE_SIZE == 0, 1);
#pragma warning(pop)