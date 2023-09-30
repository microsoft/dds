#include <stdlib.h>
#include <string.h>

#include "CacheTable.h"
#include "HashFunctions.h"

//
// The global cache table
//
//
CacheTableT* CacheTable;

//
// Initialize the cache table
//
//
int
InitCacheTable() {
    CacheTable = malloc(sizeof(CacheTableT*));
    if (!CacheTable) {
        return -1;
    }
    
    CacheTable->Table = (CacheElementT**)(malloc(sizeof(CacheElementT*) * CACHE_TABLE_BUCKET_COUNT));
    if (!CacheTable->Table) {
        return -1;
    }

    for (size_t i = 0; i != CACHE_TABLE_BUCKET_COUNT; i++) {
        CacheTable->Table[i] = (CacheElementT*)calloc(CACHE_TABLE_BUCKET_SIZE, sizeof(CacheElementT));
        if (!CacheTable->Table[i]) {
            return -1;
        }
    }

    return 0;
}

//
// Fail to insert the item, so undo its effect
//
//
static void
UndoInsert(
    CacheElementT *Carrier,
    CacheElementT *Victim,
    size_t MaxDepth,
    uint32_t Offset
) {
    uint32_t mask = CACHE_TABLE_BUCKET_COUNT - 1;
    uint32_t bucketPos;
    CacheElementT* tmp;

    for (size_t depth = 0; depth < MaxDepth; ++depth) {
        if (depth % 2) {
            bucketPos = Carrier->Hash1 & mask;
        }
        else {
            bucketPos = Carrier->Hash2 & mask;
        }

        if (Offset-- == 0) {
            Offset = CACHE_TABLE_BUCKET_SIZE - 1;
        }

        CacheElementT *begin = CacheTable->Table[bucketPos];

        //
        // Swap the elements
        //
        //
        memcpy(Victim, &begin[Offset], sizeof(CacheElementT));
        memcpy(&begin[Offset], Carrier, sizeof(CacheElementT));

        //
        // Victim becomes the carrier
        //
        //
        tmp = Carrier;
        Carrier = Victim;
        Victim = tmp;
    }
}

//
// Add an item to the cache table
//
//
int
AddToCacheTable(
    CacheItemT* Item
) {
    size_t maxDepth = (size_t) CACHE_TABLE_BUCKET_COUNT_POWER << 5;
    if (maxDepth > CACHE_TABLE_CAPACITY) {
        maxDepth = CACHE_TABLE_CAPACITY;
    }

    HashValueT hash1, hash2;
    HASH_FUNCTION1(Item->Key, sizeof(KeyT), hash1);
    HASH_FUNCTION2(Item->Key, sizeof(KeyT), hash2);
    if (hash1 == hash2) {
        hash1 = ~hash2;
    }

    uint32_t mask = CACHE_TABLE_BUCKET_COUNT - 1;
    uint32_t offset = 0;
    CacheElementT ele1, ele2;
    CacheElementT *carrier = &ele1, *victim = &ele2;
    memcpy(&carrier->Item, Item, sizeof(CacheItemT));
    carrier->Hash1 = hash1;
    carrier->Hash2 = hash2;
    uint32_t bucketPos = 0;
    
    for (size_t depth = 0; depth < maxDepth; ++depth) {
        if (depth % 2) {
            bucketPos = carrier->Hash2 & mask;
        }
        else {
            bucketPos = carrier->Hash1 & mask;
        }

        CacheElementT *begin = CacheTable->Table[bucketPos];
        CacheElementT *end = begin + CACHE_TABLE_BUCKET_SIZE;
        for (CacheElementT *elem = begin; elem != end; ++elem) {
            if (elem->Hash1 == elem->Hash2 || (elem->Hash1 & mask) != bucketPos) {
                //
                // This slot is available
                //
                //
                memcpy(&elem->Item, Item, sizeof(CacheItemT));
                elem->Hash1 = hash1;
                elem->Hash2 = hash2;

                return 0;
            }
            else if(elem->Hash1 == hash1 &&
                    elem->Hash2 == hash2 &&
                    (memcmp(&(elem->Item.Key), &(Item->Key), sizeof(KeyT)) == 0)
            ) {
                //
                // Key already exists
                // Update the value
                //
                //
                memcpy(&elem->Item, Item, sizeof(CacheItemT));

                return 0;
            }
        }

        //
        // This bucket is full, so pick one element as the victim
        //
        //
        memcpy(victim, &begin[offset], sizeof(CacheElementT));
        memcpy(&begin[offset], carrier, sizeof(CacheElementT));

        //
        // Victim becomes the carrier
        //
        //
        CacheElementT* tmp = carrier;
        carrier = victim;
        victim = tmp;

        if (++offset == CACHE_TABLE_BUCKET_SIZE) {
            offset = 0;
        }
    }

    //
    // Slot not found
    //
    //
    UndoInsert(carrier, victim, maxDepth, offset);
    if (memcmp(&carrier->Item, Item, sizeof(CacheItemT)) != 0) {
        printf("Error: failed to undo insertion\n");
    }
    return -1;
}

//
// Delete an item from the cache table
//
//
void
DeleteFromCacheTable(
    KeyT* Key
) {
    uint32_t mask = CACHE_TABLE_BUCKET_COUNT - 1;

    CacheElementT *elem, *end;
    HashValueT hash1, hash2;
    HASH_FUNCTION1(Key, sizeof(KeyT), hash1);
    HASH_FUNCTION2(Key, sizeof(KeyT), hash2);
    if (hash1 == hash2) {
        hash1 = ~hash2;
    }

    //
    // Check the first hash function
    //
    //
    elem = CacheTable->Table[hash1 & mask];
    end = elem + CACHE_TABLE_BUCKET_SIZE;
    while (elem != end) {
        if (elem->Hash1 == hash1 &&
            elem->Hash2 == hash2 &&
            (memcmp(&(elem->Item.Key), Key, sizeof(KeyT)) == 0)) {
            memset(ele, 0, sizeof(CacheElementT));
            return;
        }

        ++elem;
    }

    //
    // Check the second hash function
    //
    //
    elem = CacheTable->Table[hash2 & mask];
    end = elem + CACHE_TABLE_BUCKET_SIZE;
    while (elem != end) {
        if (elem->Hash1 == hash1 &&
            elem->Hash2 == hash2 &&
            (memcmp(&(elem->Item.Key), Key, sizeof(KeyT)) == 0)) {
            memset(ele, 0, sizeof(CacheElementT));
            return;
        }

        ++elem;
    }

    return NULL;
}

//
// Look up the cache table given a key
//
//
CacheItemT*
LookUpCacheTable(
    KeyT* Key
) {
    uint32_t mask = CACHE_TABLE_BUCKET_COUNT - 1;

    CacheElementT *elem, *end;
    HashValueT hash1, hash2;
    HASH_FUNCTION1(*Key, sizeof(KeyT), hash1);
    HASH_FUNCTION2(*Key, sizeof(KeyT), hash2);
    if (hash1 == hash2) {
        hash1 = ~hash2;
    }

    //
    // Check the first hash function
    //
    //
    elem = CacheTable->Table[hash1 & mask];
    end = elem + CACHE_TABLE_BUCKET_SIZE;
    while (elem != end) {
        if (elem->Hash1 == hash1 &&
            elem->Hash2 == hash2 &&
            (memcmp(&(elem->Item.Key), Key, sizeof(KeyT)) == 0)) {
            return &elem->Item;
        }

        ++elem;
    }

    //
    // Check the second hash function
    //
    //
    elem = CacheTable->Table[hash2 & mask];
    end = elem + CACHE_TABLE_BUCKET_SIZE;
    while (elem != end) {
        if (elem->Hash1 == hash1 &&
            elem->Hash2 == hash2 &&
            (memcmp(&(elem->Item.Key), Key, sizeof(KeyT)) == 0)) {
            return &elem->Item;
        }

        ++elem;
    }

    return NULL;
}

//
// Destroy the cache table
//
//
void
DestroyCacheTable() {
    if (CacheTable && CacheTable->Table) {
        for (size_t i = 0; i != CACHE_TABLE_BUCKET_COUNT; i++) {
            if (CacheTable->Table[i]) {
                free(CacheTable->Table[i]);
            }
        }

        free(CacheTable->Table);
    }
}