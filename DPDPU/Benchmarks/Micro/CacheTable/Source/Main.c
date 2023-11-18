#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "CacheTable.h"
#include "Profiler.h"

struct ThreadArg {
    size_t Items;
    KeyT *KeyList;
};

void* LookUpThread(void *Arg) {
    struct ThreadArg *arg = (struct ThreadArg*)Arg;
    const size_t items = arg->Items;
    KeyT *keyList = arg->KeyList;

    for (size_t i = 0; i != items; i++) {
        LookUpCacheTable(&keyList[i]);
    }

    return NULL;
}

int main(int ArgC, const char** ArgV) {
    int result = 0;
    const size_t totalItems = 20000000;
    const size_t lookUpItems = 10000000;

    if (ArgC != 2) {
        printf("Usage: %s [num threads]\n", ArgV[0]);
	return -1;
    }

    int numThreads = atoi(ArgV[1]);

    printf("Cache item size = %ld\n", sizeof(CacheItemT));

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
    for (size_t i = 0; i != totalItems; i++) {
        result = AddToCacheTable(&items[i]);
	if (!result) {
            itemCount++;
	    // printf("itemCount = %ld\n", itemCount);
	}
    }
    StopProfiler(&profiler);
    profiler.Operations = itemCount;
    
    fprintf(stdout, "%ld / %ld items added to the cache table\n", itemCount, totalItems);
    ReportProfiler(&profiler);

    /*
    CacheItemT* itemQueried;
    fprintf(stdout, "Testing if added items are correct\n");
    for (size_t i = 0; i != itemCount; i++) {
        itemQueried = LookUpCacheTable(&items[i].Key);
	if (!itemQueried) {
            fprintf(stderr, "Test failed: Key %ld doesn't exist\n", items[i].Key);
	}
        if (itemQueried->Offset != items[i].Offset) {
            fprintf(stderr, "Test failed: Key %ld (%ld -> %ld)\n", items[i].Key, items[i].Offset, itemQueried->Offset);
        }
    }
    */

    fprintf(stdout, "Looking up from the cache table...\n");

    srand(0);

    KeyT** keyLists = malloc(sizeof(KeyT*) * numThreads);
    struct ThreadArg* threadArgs = malloc(sizeof(struct ThreadArg) * numThreads);

    for (int i = 0; i != numThreads; i++) {
        keyLists[i] = malloc(sizeof(KeyT) * lookUpItems);
	for (int j = 0; j != lookUpItems; j++) {
            keyLists[i][j] = rand() % totalItems;
	}
        
	threadArgs[i].Items = lookUpItems;
	threadArgs[i].KeyList = keyLists[i];
    }

    pthread_t* threadIds = malloc(sizeof(pthread_t) * numThreads);

    StartProfiler(&profiler);

    for (int i = 0; i != numThreads; i++) {
        pthread_create(&threadIds[i], NULL, LookUpThread, &threadArgs[i]);
    }
    
    for (int i = 0; i != numThreads; i++) {
        pthread_join(threadIds[i], NULL);
    }

    StopProfiler(&profiler);
    profiler.Operations = numThreads * lookUpItems;
    
    ReportProfiler(&profiler);

    DestroyCacheTable();

    for (int i = 0; i != numThreads; i++) {
        free(keyLists[i]);
    }

    free(threadArgs);	
    free(threadIds);
    free(keyLists);

    return 0;
}
