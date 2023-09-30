#include <stdlib.h>
#include <stdio.h>

#include "CacheTable.h"
#include "Profiler.h"

int main() {
    int result = 0;
    const size_t totalItems = 2000000;

    fprintf(stdout, "Allocating test items...\n");
    CacheItemT* items = malloc(totalItems * sizeof(CacheItemT));
    if (!items) {
        fprintf(stderr, "Failed to allocate items\n");
    }

    for (size_t i = 0; i != totalItems; i++) {
        items[i].Key = i;
        items[i].Version = 42;
        items[i].FileId = 0;
        items[i].Offset = i;
        items[i].Size = 1;
    }
    
    fprintf(stdout, "Init the cache table...\n");
    result = InitCacheTable();
    if (result) {
        fprintf(stderr, "InitCacheTable failed\n");
    }

    fprintf(stdout, "Adding to the cache table...\n");
    size_t itemCount = 0;
    struct Profiler profiler;
    InitProfiler(&profiler, totalItems);

    StartProfiler(&profiler);
    while (!result && itemCount <= totalItems) {
        result = AddToCacheTable(&items[itemCount]);
        itemCount++;
    }
    StopProfiler(&profiler);
    profiler.Operations = itemCount;
    
    fprintf(stdout, "%ld items added before failure\n");
    ReportProfiler(&profiler);

    fprintf(stdout, "Testing if added items are correct");
    CacheItemT* itemQueried;
    for (size_t i = 0; i != itemCount; i++) {
        itemQueried = LookUpCacheTable(&items[i].Key)->Offset;
        if (itemQueried->Offset != items[i].Offset) {
            fprintf(stderr, "Test failed: key = %ld (%ld -> %ld)\n", items[i].Key, items[i].Offset, itemQueried->Offset);
        }
    }

    fprintf(stdout, "Looking up from the cache table...\n");
    itemCount = 0;
    StartProfiler(&profiler);
    while (itemCount <= totalItems) {
        result = LookUpCacheTable(&items[itemCount].Key);
        itemCount++;
    }
    StopProfiler(&profiler);
    profiler.Operations = itemCount;
    
    fprintf(stdout, "%ld items looked up\n");
    ReportProfiler(&profiler);

    return 0;
}