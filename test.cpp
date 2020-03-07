#include <assert.h>
#include <iostream>
#include <list>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>

#include "Cache.h"


/*
 * Generates 1-byte-offset reads, and asserts that every one except the first in
 * the cache line hits in the L1 (while the others miss the caches entirely).
 */
void test1() {
    printf("Running %s...\n", __func__);
    /* L1NLines, L1NWays, L2NLines, L2NWays, L2NBanks, cacheLineNBytes) */
    auto c = LRUCache(512, 8, 1048576, 8, 64, 64);

    size_t nBytes = 128;
    for (size_t i = 0; i < nBytes; ++i) {
        intptr_t addr = (intptr_t) i;

        c.access(addr, false);
    }

    auto s = c.getStats();
    assert(s->L1RH == nBytes - 2);  // all but initial line byte will be hits
    assert(s->L2RM == nBytes/64);   // floor of num. "first bytes in line"

    c.printStats();
    printf("%s complete.\n", __func__);
}

/*
 * Generate line-sized-offset reads, and ensure that lines that have been
 * kicked out of the L1 cache are still resident in the L2.
 */
void test2() {
    printf("Running %s...\n", __func__);
    /* L1NLines, L1NWays, L2NLines, L2NWays, L2NBanks, cacheLineNBytes) */
    auto c = LRUCache(512, 8, 1048576, 8, 8, 64);

    size_t nLines = 1048576;   // 1X the L2 cap
    size_t lineSize = 64;   // as above
    // pass 1
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;

        c.access(addr, false);
    }

    // pass 2
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;

        c.access(addr, false);
    }


    c.printStats();
    auto s = c.getStats();
    assert(s->L1RH == 0);
    assert(s->L2RM == nLines);  // from pass 1
    assert(s->L2RH == nLines);  // from pass 2


    printf("%s complete.\n", __func__);
}

/*
 * Generate accesses over an address range larger than the L2 cap, and ensure
 * that all are misses.
 */
void test3() {
    printf("Running %s...\n", __func__);
    /* L1NLines, L1NWays, L2NLines, L2NWays, L2NBanks, cacheLineNBytes) */
    auto c = LRUCache(512, 8, 1048576, 8, 64, 64);

    size_t nLines = 1048576*2;   // 2X the L2 cap
    size_t lineSize = 64;   // as above
    // pass 1
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;

        c.access(addr, false);
    }

    // pass 2
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;

        c.access(addr, false);
    }


    c.printStats();
    auto s = c.getStats();
    assert(s->L1RH == 0);
    assert(s->L2RH == 0);
    assert(s->L2RM == 2*nLines);  // from passes 1+2


    printf("%s complete.\n", __func__);
}

/*
 * Generate alternating read/write accesses to the L1, and see if their counts
 * match.
 */
void test4() {
    printf("Running %s...\n", __func__);
    /* L1NLines, L1NWays, L2NLines, L2NWays, L2NBanks, cacheLineNBytes) */
    auto c = LRUCache(512, 8, 1048576, 8, 64, 64);

    size_t nLines = 512;    // 1X L1 cap
    size_t lineSize = 64;   // as above
    // pass 1
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = i % 2 == 1;
        c.access(addr, isWrite);
    }

    // pass 2
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = i % 2 == 1;
        c.access(addr, isWrite);
    }


    c.printStats();
    auto s = c.getStats();
    assert(s->L1RH == nLines/2);
    assert(s->L1WH == nLines/2);
    assert(s->L2RM == nLines/2);
    assert(s->L2WM == nLines/2);

    printf("%s complete.\n", __func__);
}



int main(int argc, char *argv[]) {
    std::cout << "Cachesim test suite" << std::endl;

    test1();
    test2();
    test3();
    test4();

    return 0;
}
