/*
 * Header file for cache simulator module.
 *
 * NOTE: care is taken below to avoid defining virtual methods, as we don't want
 * to invoke vtable lookups for the performance-critical cachesim methods
 * (which are called on every memory access!).
 */
#pragma once

#include <list>
#include <stdbool.h>
#include <stdint.h>
#include <unordered_map>
#include <vector>

typedef uintptr_t line_addr_t;
typedef uintptr_t word_addr_t;
typedef std::unordered_map<line_addr_t, std::list<line_addr_t>::iterator> map_t;
typedef std::list<line_addr_t> list_t;

class SimpleCache {
    public:
        typedef struct {
            size_t RH, RM;
            size_t WH, WM;

            bool computedFinalStats;
            size_t nR, nW;
            size_t nH, nM, nE;
            double RHP, RMP;
            double WHP, WMP;
            double EP;
        } stats_t;

        SimpleCache(size_t nLines, size_t nWays, size_t nBanks,
                size_t cacheLineNBytes, bool allocateOnWritesOnly);
        uint64_t getCacheLineSizeLog2();
        void computeStats();
        stats_t *getStats();
        void zeroStatsCounters();
        void dumpTextStats(FILE * const outputFile);
        void dumpTextStats(const char * const outputFilepath);
        void dumpBinaryStats(const char * const outputFilepath);

        // TODO forward (to higher cache levels or memory/RAMulator)

    protected:
        size_t nLines, nWays, nSetsPerBank, nBanks;
        size_t cacheLineSizeLog2;
        bool allocateOnWritesOnly;  // act like a write-only buffer

        typedef struct {
            int64_t nReads;
            int64_t nWrites;
        } miss_stats_t;     // stored per-address in a map

        stats_t s;
        std::unordered_map<line_addr_t, miss_stats_t> misses;

        inline line_addr_t addrToLineAddr(intptr_t addr);
        inline uint32_t fastHash(line_addr_t lineAddr, uint64_t maxSize);
        inline size_t lineToLXSet(line_addr_t lineAddr, size_t nSets);
        void logMiss(line_addr_t line, bool isWrite);
};

class LRUSimpleCache : public SimpleCache {
    public:
        LRUSimpleCache(size_t nLines, size_t nWays, size_t nBanks,
                size_t cacheLineNBytes, bool allocateOnWritesOnly);
        void access(uintptr_t addr, bool isWrite);
        bool touchLine(line_addr_t lineAddr, map_t &map, list_t &list,
                size_t nWays, bool allocateOnWritesOnly, bool isWrite);


    protected:
        std::vector<std::vector<map_t>>  maps;   // 2-D vector of maps
        std::vector<std::vector<list_t>> lists;  // 2-D vector of lists

};

class Network {
    public:
        Network();
        Network(int ourGlobalRank);
        void setOurGlobalRank(int ourGlobalRank);
        void sendTo(int destID, size_t nBytes);
        void zeroStatsCounters();
        void dumpTextStats(FILE * const outputFile);
        void dumpTextStats(const char * const outputFilepath);

    private:
        int ourGlobalRank;
        std::unordered_map<int, size_t> destBytes;
};


class HistogramCounter {
    public:
        HistogramCounter(size_t bytesPerWord);
        void access(uintptr_t addr, bool isWrite);
        void zeroStatsCounters();
        void dumpBinaryStats(const char * const outputFilepath);

    private:
        typedef struct {
            int64_t nReads;
            int64_t nWrites;
        } histogram_entry_t;

        size_t bytesPerWordLog2;
        inline word_addr_t addrToWordAddr(intptr_t addr);
        std::unordered_map<word_addr_t, histogram_entry_t> hist;
};


// TODO: CompoundCache with modular forwarding
class Cache {
    public:
        typedef struct {
            size_t L1RH, L2RH, L2RM;    // note: L1RM == L2RH
            size_t L1WH, L2WH, L2WM;    // note: L1WM == L2WH

            bool computedFinalStats;
            size_t nR, nW;
            double L1RHP, L2RHP, L2RMP;
            double L1WHP, L2WHP, L2WMP;
        } stats_t;

        Cache(size_t L1NLines, size_t L1NWays, size_t L2NLines, size_t L2NWays,
                size_t L2NBanks, size_t cacheLineNBytes);
        uint64_t getCacheLineSizeLog2();
        void computeStats();
        stats_t *getStats();
        void zeroStatsCounters();
        void dumpTextStats(FILE * const outputFile);


    protected:
        size_t L1NLines, L1NWays, L1NSets;
        size_t L2NLines, L2NWays, L2NSetsPerBank, L2NBanks;
        size_t cacheLineSizeLog2;

        // interior stats struct
        stats_t s;

        inline line_addr_t addrToLineAddr(intptr_t addr);
        inline uint32_t fastHash(line_addr_t lineAddr, uint64_t maxSize);
        inline size_t lineToLXSet(line_addr_t lineAddr, size_t nSets);
        bool touchLine(line_addr_t lineAddr, map_t &map, list_t &list,
                size_t nWays);
};

class LRUCache : public Cache {
    public:
        LRUCache(size_t L1NLines, size_t L1NWays, size_t L2NLines,
                size_t L2NWays, size_t L2NBanks, size_t cacheLineNBytes);
        void access(uintptr_t addr, bool isWrite);


    protected:
        std::vector<map_t> L1Maps;                 // 1-D vector of maps
        std::vector<list_t> L1Lists;               // 1-D vector of lists
        std::vector<std::vector<map_t>>  L2Maps;   // 2-D vector of maps
        std::vector<std::vector<list_t>> L2Lists;  // 2-D vector of lists
};
