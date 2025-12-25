#ifndef PROPHET_PROFILE
#define PROPHET_PROFILE

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <cassert>
#include <random>
#include <type_traits>
#include <cstdint>
#include "champsim.h"
#include "cache.h"
#include "bakshalipour_framework.h"

#define META_TABLE_SIZE 196608
#define META_TABLE_ASSOC 12
#define MRB_TABLE_SIZE 65526
#define MRB_TABLE_ASSOC 16
#define MRB_MAX_COUNTER 3

#define IS_TRAIN 0
#define ENABLE_MRB 1



class prophet_profile;


// class BaseIndexingPolicy;
// GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
// namespace replacement_policy
// {
//     class Base;
// }
// struct ProphetPrefetcherParams;

// GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
// namespace prefetch
// {

// class ProphetHashedSetAssociative : public SetAssociative
// {
//   protected:
//     // uint32_t extractSet(const Addr addr) const override;

//   public:
//     ProphetHashedSetAssociative(
//         const ProphetHashedSetAssociativeParams &p)
//       : SetAssociative(p)
//     {
//         uint32_t numEntries = numSets * assoc;
//         uint32_t roundedBytes = numEntries * 4;
//         uint32_t actualEntries = roundedBytes / 64 * 12;
//         cout<< actualEntries << endl;
//     }
//     ~ProphetHashedSetAssociative() = default;
// };

struct SingleMetaEntry
{
    uint64_t correlatedAddr;
    SatCounter8 counter;

    SingleMetaEntry(unsigned bits) : correlatedAddr(0), counter(bits) {}
};

struct MetaEntry : public TaggedEntry
{
    /** group of stides */
    std::vector<SingleMetaEntry> entries;
    bool used;
    uint64_t pc;
    long long touch_time;
    MetaEntry() : TaggedEntry(), entries(1, 1), used(false), pc(0), touch_time(0) {};
    MetaEntry(size_t num_strides, unsigned counter_bits)
        : TaggedEntry(), entries(num_strides, counter_bits), used(false), pc(0), touch_time(0)
    {
    }

    /** Reset the entries to their initial values */
    void
    invalidate() override
    {
        TaggedEntry::invalidate();
        used = false;
        pc = 0;
        touch_time = 0;
        for (auto &entry : entries)
        {
            entry.correlatedAddr = 0;
            entry.counter.reset();
        }
    }
};

struct TrainEntry
{
    TrainEntry()
    {
        issued = 0;
        solved = 0;
        meta_inserted = 0;
        meta_used = 0;
        protect = true;
    }
    uint32_t issued;
    uint32_t solved;
    uint32_t meta_inserted;
    uint32_t meta_used;
    bool protect;
};

struct ProphetMetaTableEntry
{

    uint64_t correlatedAddr;
    // int counter;
    bool used;
    // uint64_t pc;
    ProphetMetaTableEntry() : correlatedAddr(0), used(false) {};
    ProphetMetaTableEntry(uint64_t addr) : correlatedAddr(addr) {};
};

class ProphetMetaTable : public LRUSetAssociativeCache<ProphetMetaTableEntry>
{
    typedef LRUSetAssociativeCache<ProphetMetaTableEntry> Super;

public:

     std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
    prophet_profile *pp;

    ProphetMetaTable(int size, int num_ways) : Super(size, num_ways), priority_pgo(num_sets, vector<uint8_t>(num_ways, 0)), pp(nullptr)
    {}

    void setpp(prophet_profile * p) {
        pp = p;
    }

    ProphetMetaTableEntry *find(uint64_t key)
    {
        Entry *entry = Super::find(key);
        if (!entry)
        {
            return nullptr;
        }
        return &(entry->data);
    }

    /*
        If evict another valid entry: return true!
        else: return false!
    */
    bool insert(uint64_t key, const ProphetMetaTableEntry &data, uint8_t priority = 0);

    Entry *erase(uint64_t key)
    {
        return Super::erase(key);
    }

    /* @override */
    int select_victim(uint64_t index)
    {
        uint8_t min_priority = 255;
        uint64_t min_lru = UINT64_MAX;
        int victim_index = 0;
        vector<uint8_t> &priority_set = this->priority_pgo[index];
        vector<uint64_t> &lru_set = this->lru[index];
        for (size_t i = 0; i < num_ways; i++)
        {
            if (min_priority > priority_set[i]){
                min_priority = priority_set[i];
                min_lru = lru_set[i];
                victim_index = i;
            }else if (min_priority == priority_set[i]){
                if (min_lru < lru_set[i]){
                    min_lru = lru_set[i];
                    victim_index = i;
                }
            }
        }
        return victim_index;
    }
    
    vector<vector<uint8_t>> priority_pgo;
};

struct ProphetMRBTableEntry
{
    uint64_t correlatedAddr;
    uint8_t counter;
    ProphetMRBTableEntry() : correlatedAddr(0), counter(0) {};
    ProphetMRBTableEntry(uint64_t addr) : correlatedAddr(addr), counter(0) {};
};

class ProphetMRBTable : public LRUSetAssociativeCache<ProphetMRBTableEntry>
{
    typedef LRUSetAssociativeCache<ProphetMRBTableEntry> Super;

public:
    ProphetMRBTable(int size, int num_ways) : Super(size, num_ways)
    {
        // assert(__builtin_popcount(size) == 1);
    }

    ProphetMRBTableEntry *find(uint64_t key)
    {
        Entry *entry = Super::find(key);
        if (!entry)
        {
            return nullptr;
        }
        return &(entry->data);
    }

    void insert(uint64_t key, const ProphetMRBTableEntry &data)
    {
        Super::insert(key, data);
        Super::set_mru(key);
    }

    Entry *erase(uint64_t key)
    {
        return Super::erase(key);
    }

    /* @override */
    int select_victim(uint64_t index)
    {
        uint8_t min_counter = UINT8_MAX;
        uint64_t min_lru = UINT64_MAX;
        vector<uint64_t> &lru_set = this->lru[index];
        int victim_index = 0;
        for (size_t i = 0; i < num_ways; i++)
        {
            if (!entries[index][i].valid){
                return i;
            }else if (entries[index][i].data.counter < min_counter){
                min_counter = entries[index][i].data.counter;
                victim_index = i;
            }else if (entries[index][i].data.counter == min_counter){
                if (min_lru < lru_set[i]){
                    min_lru = lru_set[i];
                    victim_index = i;
                }
            }
        }
        return victim_index;
    }
};

class prophet_profile : public champsim::modules::prefetcher
{
public:
    // BaseTags* cachetags;
    CACHE *parent = NULL;
    int debug_level = 0;
    bool inTraining = true;
    bool disablePF = false;
    std::string benchmark = champsim::global_trace_name;
    bool enableInsertFilter = true; 
    bool enablePGLRU = true;
    std::map<uint64_t, int> profileReplTable;
    std::map<uint64_t, int32_t> profileUtiTable;
    std::set<uint64_t> profileInsertTable;
    const bool enableDRA = false;
    bool enableMRB = ENABLE_MRB;
    int globalDegree = 1;
    uint32_t numEntriesinTable = 0;
    long long global_timestamp = 0;
    uint32_t allMisses = 0;
    int waysForCache = 8;
    /**
     * Information used to create a new PC table. All of them behave equally.
     */
    
    std::map<uint64_t, TrainEntry> trainTable;

    std::map<uint64_t, uint32_t> missTable; // the miss number of each PC

    ProphetMetaTable *metaTable = new ProphetMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

   

    ProphetMRBTable *mrbTable = new ProphetMRBTable(MRB_TABLE_SIZE, MRB_TABLE_ASSOC);

    std::unordered_map<uint64_t, uint64_t> pcTable; // record the last addr of PCs

    std::set<uint64_t> metaUsedPool;

    std::set<uint64_t> metaInsertedPool;

    std::map<uint64_t, uint64_t> prefetched_addr; // <block_addr, trigger pc>

    std::string out_file = "profile.txt";
    std::vector<std::string> logs;

    std::string toProfilePath(const std::string& full_path) {
        size_t last_slash = full_path.find_last_of('/');
        if (last_slash == std::string::npos) {
            return "";
        }
        // 1. 找到最后一个 '/' 的位置，分离目录和文件名
        std::string dir_part = full_path.substr(0, last_slash);
        std::string file_part = full_path.substr(last_slash + 1);
        // 2. 从 dir_part 中提取最后一级目录名（即 traces-spec2017）
        size_t second_last_slash = dir_part.find_last_of('/');
        std::string trace_dir = (second_last_slash == std::string::npos)
                                    ? dir_part
                                    : dir_part.substr(second_last_slash + 1);
        // 3. 去掉 "traces-" 前缀，得到 "spec2017"
        const std::string prefix = "traces-";
        std::string suite;
        if (trace_dir.substr(0, prefix.size()) == prefix) {
            suite = trace_dir.substr(prefix.size());
        } 
        // 4. 从 file_part 中移除最后两个扩展名（.champsimtrace.xz）
        size_t last_dot = file_part.rfind('.');
        if (last_dot == std::string::npos) {
            // 没有点，整个作为 base_name
            return "";
        }
        size_t second_last_dot = file_part.rfind('.', last_dot - 1);
        std::string base_name = (second_last_dot == std::string::npos)
                                    ? file_part.substr(0, last_dot)
                                    : file_part.substr(0, second_last_dot);

        std::string profile_path = "/mnt/data/lyq/Kairos2/expr/log/baseline/"+ base_name + ".txt";
        return profile_path;

    }



    void set_llc_reference(CACHE* llc) {
        parent = llc;
    }

    void addToUsedPool(uint64_t a)
    {
        if (metaUsedPool.find(a) == metaUsedPool.end())
        {
            metaUsedPool.insert(a);
        }
    }

    void addToInsertedPool(uint64_t a)
    {
        if (metaInsertedPool.find(a) == metaInsertedPool.end())
        {
            metaInsertedPool.insert(a);
        }
    }

    bool isAlreadyInQueue(std::vector<uint64_t> &addresses, uint64_t addr)
    {
        for (uint64_t &a : addresses)
        {
            if (a == addr)
                return true;
        }
        return false;
    }

    int issue_metatable(ProphetMetaTable *metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t> &addresses);
    int issue_mrbtable(ProphetMRBTable *metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t> &addresses);

    void invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr);

    void outPrefetcherPGOInfo();

    uint64_t get_last(uint64_t ip) {
        if(pcTable.find(ip) != pcTable.end()) {
            return pcTable[ip];
        }else {
            return 0;
        }
    };

    std::set<uint64_t> get_triggers(uint64_t target) {
        if(metaTable->reverse_metatable.find(target) != metaTable->reverse_metatable.end()){
            return metaTable->reverse_metatable[target];
        } else {
            return std::set<uint64_t>();
        }
    };





    using champsim::modules::prefetcher::prefetcher;

    void prefetcher_initialize(){
        metaTable->setpp(this);
        benchmark = champsim::global_trace_name;
    
        globalDegree = 1;
        enableMRB = false;

        out_file = toProfilePath(benchmark);
        
        cout << out_file << endl;
    }

    uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                        uint32_t metadata_in, std::string latepf );
    uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
    void prefetcher_cycle_operate();
    void prefetcher_final_stats();
};

// } // namespace prefetch
// } // namespace gem5

#endif // __MEM_CACHE_PREFETCH_Prophet_HH__
