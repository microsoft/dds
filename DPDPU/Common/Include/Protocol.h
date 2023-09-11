#pragma once

#define DDS_ERROR_CODE_SUCCESS 0
#define DDS_ERROR_CODE_TOO_MANY_DIRS 1
#define DDS_ERROR_CODE_TOO_MANY_FILES 2
#define DDS_ERROR_CODE_DIR_EXISTS 3
#define DDS_ERROR_CODE_DIR_NOT_FOUND 4
#define DDS_ERROR_CODE_DIR_NOT_EMPTY 5
#define DDS_ERROR_CODE_FILE_EXISTS 6
#define DDS_ERROR_CODE_FILE_NOT_FOUND 7
#define DDS_ERROR_CODE_READ_OVERFLOW 8
#define DDS_ERROR_CODE_WRITE_OVERFLOW 9
#define DDS_ERROR_CODE_BUFFER_OVERFLOW 10
#define DDS_ERROR_CODE_NOT_IMPLEMENTED 11
#define DDS_ERROR_CODE_REQUIRE_POLLING 12
#define DDS_ERROR_CODE_TOO_MANY_POLLS 13
#define DDS_ERROR_CODE_INVALID_POLL_DELETION 14
#define DDS_ERROR_CODE_IO_PENDING 15
#define DDS_ERROR_CODE_STORAGE_OUT_OF_SPACE 16
#define DDS_ERROR_CODE_INVALID_FILE_POSITION 17
#define DDS_ERROR_CODE_UNEXPECTED_MSG 18
#define DDS_ERROR_CODE_FAILED_CONNECTION 19
#define DDS_ERROR_CODE_FAILED_BUFFER_ALLOCATION 20
#define DDS_ERROR_CODE_OOM 21
#define DDS_ERROR_CODE_TOO_MANY_REQUESTS 22
#define DDS_ERROR_CODE_REQUEST_RING_FAILURE 23

#define DDS_CACHE_LINE_SIZE 64
#define DDS_CACHE_LINE_SIZE_BY_INT 16
#define DDS_CACHE_LINE_SIZE_BY_LONGLONG 8
#define DDS_CACHE_LINE_SIZE_BY_POINTER 8

#define DDS_MAX_FILE_PATH 64
#define DDS_MAX_DEVICE_NAME_LEN 32
#define DDS_MAX_DIRS 2
#define DDS_MAX_FILES 1
#define DDS_MAX_POLLS 1
#define DDS_DIR_INVALID 0xFFFFUL
#define DDS_DIR_ROOT 0
#define DDS_FILE_INVALID 0xFFFFUL
#define DDS_PAGE_SIZE 4096
#define DDS_POLL_DEFAULT 0
#define DDS_POLL_MAX_LATENCY_MICROSECONDS 100
#define DDS_REQUEST_RING_SIZE 20480
#define DDS_REQUEST_RING_SLOT_SIZE DDS_PAGE_SIZE
#define DDS_REQUEST_RING_BYTES 83886080
#define DDS_RESPONSE_RING_SIZE 30720
#define DDS_RESPONSE_RING_SLOT_SIZE DDS_PAGE_SIZE
#define DDS_RESPONSE_RING_BYTES 125829120
#define DDS_FRONTEND_MAX_OUTSTANDING 128

#define RING_BUFFER_REQUEST_MAXIMUM_TAIL_ADVANCEMENT 1048576
#define BACKEND_REQUEST_MAX_DMA_SIZE RING_BUFFER_REQUEST_MAXIMUM_TAIL_ADVANCEMENT
#define BACKEND_REQUEST_BUFFER_SIZE DDS_REQUEST_RING_BYTES
#define RING_BUFFER_RESPONSE_MINIMUM_TAIL_ADVANCEMENT 8192
#define RING_BUFFER_RESPONSE_MAXIMUM_TAIL_ADVANCEMENT 1048576
#define BACKEND_RESPONSE_MAX_DMA_SIZE RING_BUFFER_RESPONSE_MAXIMUM_TAIL_ADVANCEMENT
#define BACKEND_RESPONSE_BUFFER_SIZE DDS_RESPONSE_RING_BYTES
#define RING_BUFFER_RESPONSE_BATCH_ENABLED

#define RING_BUFFER_REQUEST_META_DATA_SIZE 128
#define RING_BUFFER_RESPONSE_META_DATA_SIZE 128

//
// Check a few parameters at the compile time
//
//
#define assert_static_protocol(e, num) \
    enum { assert_static_protocol__##num = 1/(e) }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result" 
#else
#pragma warning(push)
#pragma warning (disable: 4804)
#endif
assert_static_protocol(RING_BUFFER_REQUEST_META_DATA_SIZE == DDS_CACHE_LINE_SIZE * 2, 0);
assert_static_protocol(RING_BUFFER_RESPONSE_META_DATA_SIZE == DDS_CACHE_LINE_SIZE * 2, 1);
assert_static_protocol(DDS_REQUEST_RING_BYTES == DDS_REQUEST_RING_SIZE * DDS_REQUEST_RING_SLOT_SIZE, 2);
assert_static_protocol(DDS_REQUEST_RING_BYTES % DDS_CACHE_LINE_SIZE == 0, 3);
assert_static_protocol(DDS_RESPONSE_RING_BYTES == DDS_RESPONSE_RING_SIZE * DDS_RESPONSE_RING_SLOT_SIZE, 4);
assert_static_protocol(DDS_RESPONSE_RING_BYTES % DDS_CACHE_LINE_SIZE == 0, 5);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif

#define DDS_BACKEND_ADDR "172.16.1.4"
#define DDS_BACKEND_PORT 4242
