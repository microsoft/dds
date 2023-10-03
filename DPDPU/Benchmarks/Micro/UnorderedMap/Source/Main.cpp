#include <stdlib.h>
#include <stdio.h>
#include <unordered_map>

#include "CacheTable.h"
#include "ProfilerCC.h"

using std::unordered_map;

int main() {
    const size_t totalItems = 20000000;

    fprintf(stdout, "Allocating test items...\n");
    CacheItemT* items = (CacheItemT*)malloc(totalItems * sizeof(CacheItemT));
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
    
    fprintf(stdout, "Init the map...\n");
    unordered_map<KeyT, CacheItemT> table;
    table.reserve(CACHE_TABLE_CAPACITY);

    fprintf(stdout, "Adding to the map...\n");
    size_t itemCount = 0;
    Profiler profiler(totalItems);

    profiler.Start();
    for (size_t i = 0; i != totalItems; i++) {
	table[items[i].Key] = items[i];
	itemCount++;
    }
    profiler.Stop();
    
    fprintf(stdout, "%ld / %ld items added to the cache table\n", itemCount, totalItems);
    profiler.Report();

    fprintf(stdout, "Looking up from the cache table...\n");
    itemCount = 0;
    CacheItemT* itemQueried;
    profiler.Start();
    while (itemCount != totalItems) {
        itemQueried = &table.find(items[itemCount].Key)->second;
	if (itemQueried->Offset != items[itemCount].Offset) {
		fprintf(stderr, "Incorrect result\n");
	}
        itemCount++;
    }
    profiler.Stop();
    
    fprintf(stdout, "%ld items looked up\n", itemCount);
    profiler.Report();

    free(items);

    return 0;
}
