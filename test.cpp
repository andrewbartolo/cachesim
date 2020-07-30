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

    c.dumpTextStats(stderr);
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


    c.dumpTextStats(stderr);
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


    c.dumpTextStats(stderr);
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


    c.dumpTextStats(stderr);
    auto s = c.getStats();
    assert(s->L1RH == nLines/2);
    assert(s->L1WH == nLines/2);
    assert(s->L2RM == nLines/2);
    assert(s->L2WM == nLines/2);

    printf("%s complete.\n", __func__);
}

/*
 *
 */
void test5() {
    printf("Running %s...\n", __func__);

    /* nLines, nWays, nBanks, cacheLineNBytes, allocateOnWritesOnly */
    auto c = LRUSimpleCache(1048576, 8, 1, 64, true, false);

    size_t nLines = 1048576;
    size_t lineSize = 64;

    // pass 1
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = false;
        c.access(addr, isWrite);
    }
    // pass 2
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = false;
        c.access(addr, isWrite);
    }

    // ensure that all of these are misses (since reads don't fault in)
    c.dumpTextStats(stderr);
    auto s = c.getStats();
    assert(s->RH == 0);

    printf("%s complete.\n", __func__);
}


/*
 *
 */
void test6() {
    printf("Running %s...\n", __func__);

    /* nLines, nWays, nBanks, cacheLineNBytes, allocateOnWritesOnly */
    auto c = LRUSimpleCache(1048576, 8, 1, 64, true, false);

    size_t nLines = 1048576;
    size_t lineSize = 64;

    // pass 1: all writes
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = false;
        c.access(addr, isWrite);
    }
    // pass 2: all writes
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = true;
        c.access(addr, isWrite);
    }
    // pass 3: all reads
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = false;
        c.access(addr, isWrite);
    }
    // pass 4: all writes again
    for (size_t i = 0; i < nLines; ++i) {
        intptr_t addr = i * lineSize;
        bool isWrite = true;
        c.access(addr, isWrite);
    }


    // ensure that number of hit
    c.dumpTextStats(stderr);
    auto s = c.getStats();

    assert(s->RM == nLines);
    assert(s->WM == nLines);
    assert(s->RH == nLines);
    assert(s->WH == nLines);

    printf("%s complete.\n", __func__);
}


int main(int argc, char *argv[]) {
    std::cout << "Cachesim test suite" << std::endl;

    // LRUCache tests
    #if 0
    test1();
    test2();
    test3();
    test4();
    #endif

    //LRUSimpleCache tests
    test5();
    test6();

    return 0;
}
