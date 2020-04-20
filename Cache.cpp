/*
 * Implementation of cache simulator module.
 */
#include <assert.h>
#include <fstream>
#include <iostream>
#include <list>
#include <math.h>
#include <sstream>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "Cache.h"


/* Base class definitions */
SimpleCache::SimpleCache(size_t nLines, size_t nWays, size_t nBanks,
        size_t cacheLineNBytes, bool allocateOnWritesOnly) {
    this->nLines = nLines;
    this->nWays = nWays;
    assert(nLines % nWays == 0);
    this->nBanks = nBanks;
    assert(nLines % nBanks == 0);
    this->nSetsPerBank = (nLines / nBanks) / nWays;

    // zero the stats struct
    memset(&this->s, 0, sizeof(this->s));

    this->cacheLineSizeLog2 = log2(cacheLineNBytes);
    this->allocateOnWritesOnly = allocateOnWritesOnly;
}

inline uint32_t SimpleCache::fastHash(line_addr_t lineAddr, uint64_t maxSize) {
    uint32_t res = 0;
    uint64_t tmp = lineAddr;
    for (uint32_t i = 0; i < 4; i++) {
        res ^= (uint32_t) ( ((uint64_t)0xffff) & tmp);
        tmp = tmp >> 16;
    }
    return (res % maxSize);
}

inline line_addr_t SimpleCache::addrToLineAddr(intptr_t addr) {
    return addr >> cacheLineSizeLog2;
}

inline size_t SimpleCache::lineToLXSet(line_addr_t lineAddr, size_t nSets) {
    return lineAddr & (line_addr_t(nSets-1));
}

/*
 * Uses a hash map counter to keep count of how many times a region of backing
 * memory has been evicted to.
 */
void SimpleCache::logEvictedLine(line_addr_t line) {
    ++evictedLines[line];
}

/*
 * "Touches" (emplaces) a line in the cache determined by the passed-in
 * (map, list, n_ways). Parameterized this way to support L1, L2, etc.
 *
 * Return value: whether/not the touch action was a hit.
 */
bool SimpleCache::touchLine(line_addr_t line, map_t &map, list_t &list,
        size_t nWays, bool allocateOnWritesOnly, bool isWrite) {
    bool wasInCache = map.count(line) != 0;

    bool shouldEvict = !allocateOnWritesOnly or
            (allocateOnWritesOnly and isWrite);

    bool shouldAppend = shouldEvict or wasInCache;

    // delete ourself (will re-add at list end later)
    // Note: we need to update our timestamp (order) regardless of R/W
    if (wasInCache) {
        list.erase(map[line]);
        map.erase(line);
    }
    else if (map.size() == nWays and shouldEvict) {  // kick somebody else out
        // evict the head of the list (LRU)
        auto otherItToEvict = list.begin();
        auto otherToEvict = *otherItToEvict;

        list.erase(otherItToEvict);
        map.erase(otherToEvict);

        ++s.nE;     // record the eviction
        logEvictedLine(otherToEvict);
    }

    // "touch" (emplace the line at back) to update it for LRU
    if (shouldAppend) {
        list.emplace_back(line);
        auto insertedIt = std::prev(list.end());  // get the last actual element
        map.emplace(line, insertedIt);
    }

    return wasInCache;
}

uint64_t SimpleCache::getCacheLineSizeLog2() {
    return cacheLineSizeLog2;
}

void SimpleCache::computeStats() {
    s.nR = s.RH + s.RM;
    s.nW = s.WH + s.WM;

    s.nH = s.RH + s.WH;
    s.nM = s.RM + s.WM;

    if (s.nR != 0) {
        s.RHP = double(s.RH) / double(s.nR);
        s.RMP = double(s.RM) / double(s.nR);
    }
    if (s.nW != 0) {
        s.WHP = double(s.WH) / double(s.nW);
        s.WMP = double(s.WM) / double(s.nW);
    }

    if (s.nM != 0) {
        s.EP = double(s.nE) / double(s.nM);
    }

    s.computedFinalStats = true;
}

SimpleCache::stats_t *SimpleCache::getStats() {
    return &s;
}

/*
 * Used for terminating the warmup phase. Zeroes stats counters while leaving
 * the maps and lists that actually store the accessed locations intact.
 */
void SimpleCache::zeroStatsCounters() {
    memset(&s, 0, sizeof(s));
    evictedLines.clear();
}

void SimpleCache::printStats() {
    if (!s.computedFinalStats) {
        std::cerr << "Stats not computed yet; computing..." << std::endl;
        computeStats();
    }

    std::cerr << "------------ Cache Statistics ------------" << std::endl;
    // use printf, since it's nicer
    fprintf(stderr, "READ_HITS\t%zu (%.2f%%)\n", s.RH, s.RHP*100);
    fprintf(stderr, "WRITE_HITS\t%zu (%.2f%%)\n", s.WH, s.WHP*100);
    fprintf(stderr, "READ_MISSES\t%zu (%.2f%%)\n", s.RM, s.RMP*100);
    fprintf(stderr, "WRITE_MISSES\t%zu (%.2f%%)\n", s.WM, s.WMP*100);
    fprintf(stderr, "EVICTIONS\t%zu (%.2f%%)\n", s.nE, s.EP*100);
}

void SimpleCache::dumpBinaryStats(char *outputDir) {
    fprintf(stderr, "There were %zu addrs in the evictions log\n",
            evictedLines.size());

    std::stringstream outFilename;
    // use the caller's own TID as a cheap way of disambiguating dumps from
    // multiple threads.
    outFilename << outputDir << "/" << syscall(SYS_gettid) << ".npbin";

    std::ofstream of(outFilename.str(), std::ios::out | std::ios::binary);

    for (auto &kv : evictedLines) {
        // write a 2-tuple (addr, count)
        of.write((char *)&kv.first, sizeof(kv.first));
        of.write((char *)&kv.second, sizeof(kv.second));
    }

    of.close();

#if 0
    for (auto &kv : evictedLines) {
        fprintf(stderr, "L0x%lx : %zd\n", kv.first, kv.second);
    }
#endif
}


/* Derived class definitions */
LRUSimpleCache::LRUSimpleCache(size_t nLines, size_t nWays, size_t nBanks,
        size_t cacheLineNBytes, bool allocateOnWritesOnly) : SimpleCache(
        nLines, nWays, nBanks, cacheLineNBytes, allocateOnWritesOnly) {

    // initialize the 2-D maps + lists (to support banks)
    maps = std::vector<std::vector<map_t>>(nBanks,
            std::vector<map_t>(nSetsPerBank));
    lists = std::vector<std::vector<list_t>>(nBanks,
            std::vector<list_t>(nSetsPerBank));

    std::cerr << "done initializing data structures" << std::endl;
}

void LRUSimpleCache::access(uintptr_t addr, bool isWrite) {
    line_addr_t lineAddr = addrToLineAddr(addr);

    // NOTE: want constant propagation w/these, may not get it
    // Why do we only take LSBs for set, but hash banks?
    // 1. Because sets are about optimizing for capacity utilization, and
    // 2. Because banks are about optimizing for concurrency
    size_t set = lineToLXSet(lineAddr, nSetsPerBank);
    size_t bank = fastHash(lineAddr, nBanks);

    // retrieve the correct map and list for the Way
    auto &map = maps[bank][set];
    auto &list = lists[bank][set];

    bool wasHit = touchLine(lineAddr, map, list, nWays, allocateOnWritesOnly,
            isWrite);

    // record stats
    if (!isWrite) wasHit ? ++s.RH : ++s.RM;
    else          wasHit ? ++s.WH : ++s.WM;
}









/* Base class definitions */
Cache::Cache(size_t L1NLines, size_t L1NWays, size_t L2NLines, size_t L2NWays,
        size_t L2NBanks, size_t cacheLineNBytes) {
    this->L1NLines = L1NLines;
    this->L1NWays = L1NWays;
    assert(L1NLines % L1NWays == 0);
    this->L1NSets = L1NLines / L1NWays;

    this->L2NLines = L2NLines;
    this->L2NWays = L2NWays;
    assert(L2NLines % L2NWays == 0);
    assert(L2NLines % L2NBanks == 0);
    this->L2NBanks = L2NBanks;
    this->L2NSetsPerBank = (L2NLines / L2NBanks) / L2NWays;

    // zero the stats struct
    memset(&this->s, 0, sizeof(this->s));

    this->cacheLineSizeLog2 = log2(cacheLineNBytes);
}

inline uint32_t Cache::fastHash(line_addr_t lineAddr, uint64_t maxSize) {
    uint32_t res = 0;
    uint64_t tmp = lineAddr;
    for (uint32_t i = 0; i < 4; i++) {
        res ^= (uint32_t) ( ((uint64_t)0xffff) & tmp);
        tmp = tmp >> 16;
    }
    return (res % maxSize);
}

inline line_addr_t Cache::addrToLineAddr(intptr_t addr) {
    return addr >> cacheLineSizeLog2;
}

inline size_t Cache::lineToLXSet(line_addr_t lineAddr, size_t nSets) {
    return lineAddr & (line_addr_t(nSets-1));
}

/*
 * "Touches" (emplaces) a line in the cache determined by the passed-in
 * (map, list, n_ways). Parameterized this way to support L1, L2, etc.
 *
 * Return value: whether/not the touch action was a hit.
 */
bool Cache::touchLine(line_addr_t line, map_t &map, list_t &list,
        size_t nWays) {
    bool wasInCache = map.count(line) != 0;


    if (wasInCache) {     // delete ourself (will re-add at list end later)
        list.erase(map[line]);
        map.erase(line);
    }
    else if (map.size() == nWays) {  // kick somebody else out
        // evict the head of the list (LRU)
        auto otherItToEvict = list.begin();
        auto otherToEvict = *otherItToEvict;

        list.erase(otherItToEvict);
        map.erase(otherToEvict);
    }

    // "touch" (emplace the line at back) regardless
    list.emplace_back(line);
    auto insertedIt = std::prev(list.end());  // get the last actual element
    map.emplace(line, insertedIt);

    return wasInCache;
}

uint64_t Cache::getCacheLineSizeLog2() {
    return cacheLineSizeLog2;
}

void Cache::computeStats() {
    s.nR = s.L1RH + s.L2RH + s.L2RM;
    s.nW = s.L1WH + s.L2WH + s.L2WM;

    if (s.nR != 0) {
        s.L1RHP = double(s.L1RH) / double(s.nR);
        s.L2RHP = double(s.L2RH) / double(s.nR);
        s.L2RMP = double(s.L2RM) / double(s.nR);
    }
    if (s.nW != 0) {
        s.L1WHP = double(s.L1WH) / double(s.nW);
        s.L2WHP = double(s.L2WH) / double(s.nW);
        s.L2WMP = double(s.L2WM) / double(s.nW);
    }

    s.computedFinalStats = true;
}

Cache::stats_t *Cache::getStats() {
    return &s;
}

/*
 * Used for terminating the warmup phase. Zeroes stats counters while leaving
 * the maps and lists that actually store the accessed locations intact.
 */
void Cache::zeroStatsCounters() {
    memset(&s, 0, sizeof(s));
}

void Cache::printStats() {
    if (!s.computedFinalStats) {
        std::cerr << "Stats not computed yet; computing..." << std::endl;
        computeStats();
    }

    std::cerr << "------------ Cache Statistics ------------" << std::endl;
    // use printf, since it's nicer
    fprintf(stderr, "L1:    RH: %zu (%.2f%%)    WH: %zu (%.2f%%)\n", s.L1RH,
            s.L1RHP*100, s.L1WH, s.L1WHP*100);
    fprintf(stderr, "L2:    RH: %zu (%.2f%%)    WH: %zu (%.2f%%)\n", s.L2RH,
            s.L2RHP*100, s.L2WH, s.L2WHP*100);
    fprintf(stderr, "Mem:   RH: %zu (%.2f%%)    WH: %zu (%.2f%%)\n", s.L2RM,
            s.L2RMP*100, s.L2WM, s.L2WMP*100);
}


/* Derived class definitions */
LRUCache::LRUCache(size_t L1NLines, size_t L1NWays, size_t L2NLines,
        size_t L2NWays, size_t L2NBanks, size_t cacheLineNBytes) :
        Cache(L1NLines, L1NWays, L2NLines, L2NWays, L2NBanks, cacheLineNBytes) {

    // initialize the 1-D maps + lists (no banks in L1)
    L1Maps = std::vector<map_t>(L1NSets);
    L1Lists = std::vector<list_t>(L1NSets);

    // initialize the 2-D maps + lists (b/c L2 has banks)
    L2Maps = std::vector<std::vector<map_t>>(L2NBanks,
            std::vector<map_t>(L2NSetsPerBank));
    L2Lists = std::vector<std::vector<list_t>>(L2NBanks,
            std::vector<list_t>(L2NSetsPerBank));

    std::cerr << "done initializing data structures" << std::endl;
}

void LRUCache::access(uintptr_t addr, bool isWrite) {
    line_addr_t lineAddr = addrToLineAddr(addr);

    // NOTE: want constant propagation w/these, may not get it
    size_t L1Set = lineToLXSet(lineAddr, L1NSets);
    size_t L2Bank = fastHash(lineAddr, L2NBanks);
    size_t L2Set = lineToLXSet(lineAddr, L2NSetsPerBank);

    // retrieve the correct map and list for the Way
    auto &L1Map = L1Maps[L1Set];
    auto &L1List = L1Lists[L1Set];
    auto &L2Map = L2Maps[L2Bank][L2Set];
    auto &L2List = L2Lists[L2Bank][L2Set];

    bool wasL1Hit = touchLine(lineAddr, L1Map, L1List, L1NWays);
    bool wasL2Hit = touchLine(lineAddr, L2Map, L2List, L2NWays);

    if (!isWrite) {
        wasL1Hit ? ++s.L1RH : wasL2Hit ? ++s.L2RH : ++s.L2RM;
    }
    else {  // was a write
        wasL1Hit ? ++s.L1WH : wasL2Hit ? ++s.L2WH : ++s.L2WM;
    }

}
