#pragma once

#include "BackEndTypes.h"

//
// Total items allowed in the cache table 
// Note: 128GB contains 16777216 8KB pages
//
//
#define CACHE_TABLE_CAPACITY 33554432
#define CACHE_TABLE_BUCKET_SIZE 4
#define CACHE_TABLE_BUCKET_COUNT 8388608
#define CACHE_TABLE_BUCKET_COUNT_POWER 23

//
// Define hash value type
//
//
typedef uint32_t HashValueT;

//
// Define key type
// TODO: Template this type and the one below
//
//
typedef size_t KeyT;

//
// Define cache item type
//
//
typedef struct {
    KeyT Key;
    size_t Version;
    FileIdT FileId;
    FileSizeT Offset;
    FileIOSizeT Size;
} CacheItemT;

//
// Define cache element type
// We need two hash values for cuckoo hashing
//
//
typedef struct {
    CacheItemT Item;
    HashValueT Hash1;
    HashValueT Hash2;
} CacheElementT;

//
// Define cache table type
//
//
typedef struct {
    CacheElementT** Table;
} CacheTableT;

//
// The global cache table
//
//
extern CacheTableT* CacheTable;

//
// Initialize the cache table
//
//
int
InitCacheTable();

//
// Add an item to the cache table
//
//
int
AddToCacheTable(
    CacheItemT* Item
);

//
// Delete an item from the cache table
//
//
void
DeleteFromCacheTable(
    KeyT* Key
);

//
// Look up the cache table given a key
//
//
CacheItemT*
LookUpCacheTable(
    KeyT* Key
);

//
// Destroy the cache table
//
//
void
DestroyCacheTable();

//
// Check a few parameters at the compile time
//
//
#define AssertStaticCacheTable(e, num) \
    enum { AssertStaticCacheTable__##num = 1/(e) }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result" 
#else
#pragma warning(push)
#pragma warning (disable: 4804)
#endif

AssertStaticCacheTable(CACHE_TABLE_CAPACITY == CACHE_TABLE_BUCKET_SIZE * CACHE_TABLE_BUCKET_COUNT, 0);
AssertStaticCacheTable((CACHE_TABLE_BUCKET_SIZE & (CACHE_TABLE_BUCKET_SIZE - 1)) == 0, 1);
AssertStaticCacheTable(1 << CACHE_TABLE_BUCKET_COUNT_POWER == CACHE_TABLE_BUCKET_COUNT, 2);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
