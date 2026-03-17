#ifndef PRISM
#define PRISM

#include <cassert>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "bakshalipour_framework.h"
#include "cache.h"
#include "champsim.h"

#define PC_TRIGGER_PREFETCHING
#define PC_TRIGGER_PREFETCHING_FEEDBACK_CONTROL
#define PCQ_SIZE 8
#define PC_META_TABLE_SIZE 4096 * 1 * 12
#define PC_META_TABLE_ASSOC 12

// #define CONFLICT_PREFETCHING
#define CONFLICT_META_TABLE_SIZE 4096 * 1 * 12
#define CONFLICT_META_TABLE_ASSOC 12

#define INIT_RESIZE
#define INIT_RESIZE_WINDOW 10000
// #define RUNTIME_RESIZE
// #define RUNTIME_RESIZE_WINDOW 100000000

// regular components
#define DEFAULT_LOOKAHEAD 3
#define DEFAULT_DEGREE 3

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16
#define PC_TABLE_TAG_WIDTH 12

#define META_TABLE_SIZE 4096 * 8 * 12
#define META_TABLE_ASSOC 12

#define PREFETCH_FILTER
#define PF_FILTER_SIZE 512
#define PF_FILTER_ASSOC 8
#define PF_FILTER_TAG_WIDTH 10

class prism;

uint64_t hash_xor(uint64_t key);

struct MetaTableEntry {
public:
  uint64_t target_addr;
  bool confident;
  MetaTableEntry(uint64_t addr = 0) : target_addr(addr), confident(false) {};
};

class prismMetaTable : public SHIPSetAssociativeCache<MetaTableEntry>
{
  typedef SHIPSetAssociativeCache<MetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  prism* prefetcher;

  prismMetaTable(int size, int num_ways) : Super(size, num_ways) {}

  void setpp(prism* p) { prefetcher = p; }

  MetaTableEntry* find(uint64_t key)
  {
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    return &(entry->data);
  }

  bool insert(uint64_t key, uint64_t ip, const MetaTableEntry& data);

  void insert_for_resize(uint64_t key, const MetaTableEntry& data, uint64_t rrpv_value);
};

class PF_Filter : public FIFOSetAssociativeCache<uint64_t>
{
  typedef FIFOSetAssociativeCache<uint64_t> Super;

public:
  const uint64_t mask;
  PF_Filter(int size, int num_ways, int debug_level = 0)
      : Super(size, num_ways, debug_level), mask((1ULL << (__builtin_ctz(size) + PF_FILTER_TAG_WIDTH)) - 1) {};

  void insert(uint64_t addr)
  {
    uint64_t key = build_key(addr);
    Super::insert(key, addr);
  }

  uint64_t* find(uint64_t addr)
  {
    uint64_t key = build_key(addr);
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    return &(entry->data);
  };

  // may broke FIFO
  void erase(uint64_t addr)
  {
    uint64_t key = build_key(addr);
    Super::erase(key);
  }

  uint64_t build_key(uint64_t addr)
  {
    uint64_t key = hash_xor(addr);
    key &= mask;
    return key;
  };
};

class prism : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;
  int waysForCache = 8;
  bool warmup_complete = false;
  std::string log_file_name;
  std::ofstream logfile;

  struct PCTableEntry {
    uint64_t lookahead;
    uint64_t degree;
    uint64_t latePrefetchCount;
    uint64_t usefulPrefetchCount;
    uint64_t filledPrefetchCount;
    uint64_t missCount;
    bool modified;
    deque<uint64_t> addrHistory;

    PCTableEntry(uint64_t _last_addr = 0, bool cache_hit = false)
        : lookahead(DEFAULT_LOOKAHEAD), degree(DEFAULT_DEGREE), latePrefetchCount(0), usefulPrefetchCount(0), filledPrefetchCount(0),
          missCount(cache_hit ? 0 : 1), modified(false)
    {
      addrHistory.push_front(_last_addr);
    };
  };

  LRUSetAssociativeCache<PCTableEntry>* pcTable = new LRUSetAssociativeCache<PCTableEntry>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  prismMetaTable* mainMetaTable = new prismMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);
  // stat
  uint64_t main_table_lookups = 0;
  uint64_t main_table_hits = 0;
  uint64_t main_table_issued_prefetches = 0;
  uint64_t main_table_accurate_prefetches = 0;
  std::set<uint64_t> main_table_prefetches;

#ifdef PC_TRIGGER_PREFETCHING
  uint64_t lack_Trigger_Num = 0;
  uint64_t unmodified_PC_Num = 0;
  bool enable_PC_Trigger_prefetching = false;
  std::deque<uint64_t> PCQ;
  SRRIPSetAssociativeCache<MetaTableEntry>* pcMetaTable = new SRRIPSetAssociativeCache<MetaTableEntry>(PC_META_TABLE_SIZE, PC_META_TABLE_ASSOC);
  // stat
  uint64_t PCM_late_prefetches = 0;
  uint64_t PCM_useful_prefetches = 0;
  uint64_t PCM_useless_prefetches = 0;
  std::set<uint64_t> PCM_issued_prefetches;
  std::set<uint64_t> PCM_filled_prefetches;
#endif

#ifdef CONFLICT_PREFETCHING
  SRRIPSetAssociativeCache<MetaTableEntry>* conflictMetaTable =
      new SRRIPSetAssociativeCache<MetaTableEntry>(CONFLICT_META_TABLE_SIZE, CONFLICT_META_TABLE_ASSOC);
  // stat
  uint64_t conflict_table_lookups = 0;
  uint64_t conflict_table_hits = 0;
  uint64_t conflict_table_issued_prefetches = 0;
  uint64_t conflict_table_accurate_prefetches = 0;
  std::set<uint64_t> conflict_table_prefetches;
#endif

#ifdef INIT_RESIZE
  uint64_t resize_useful_prefetch = 0;
  uint64_t resize_issued_prefetch = 0;
  uint64_t resize_demand = 0;
  float resize_score, resize_llc_hit_rate, resize_useful_prefetch_rate;
  bool init_resized = false;
  void reset_metadata_size()
  {
    using hits_value_type = typename decltype(llc_cache->sim_stats.hits)::value_type;
    using misses_value_type = typename decltype(llc_cache->sim_stats.misses)::value_type;

    long llc_hits = llc_cache->sim_stats.hits.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, hits_value_type{});
    long llc_misses = llc_cache->sim_stats.misses.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, misses_value_type{});
    resize_llc_hit_rate = (1.0 * llc_hits / (llc_hits + llc_misses));
    if (resize_issued_prefetch)
      resize_useful_prefetch_rate = (1.0 * resize_useful_prefetch / resize_issued_prefetch);
    else
      resize_useful_prefetch_rate = 0;
    resize_score = 2 * resize_llc_hit_rate - 1 * resize_useful_prefetch_rate;
    if (resize_score > 0) {
      // resize llc cache
      waysForCache = 14;
      llc_cache->set_available_ways(waysForCache);
      // resize markov table
      prismMetaTable* resized_metadata_table = new prismMetaTable(4096 * META_TABLE_ASSOC * 2, META_TABLE_ASSOC);
      for (int i = 0; i < mainMetaTable->num_sets; i++) {

        if (i % 8 == 0 || i % 8 == 1) {
          for (int j = 0; j < mainMetaTable->entries[i].size(); j++) {
            if (mainMetaTable->entries[i][j].valid)
              resized_metadata_table->insert_for_resize(mainMetaTable->entries[i][j].key, mainMetaTable->entries[i][j].data, mainMetaTable->rrpv[i][j]);
          }
        }
      }
      resized_metadata_table->sampler = mainMetaTable->sampler;
      resized_metadata_table->shct = mainMetaTable->shct;
      resized_metadata_table->access_count = mainMetaTable->access_count;
      delete mainMetaTable;
      mainMetaTable = resized_metadata_table;
      cout << "Resize Metadata table. Alloc 2 ways for metadata!" << endl;
    } else {
      cout << "Donot Resize Metadata table. Alloc 8 ways for metadata!" << endl;
    }
    cout << "Unmod_PC " << unmodified_PC_Num << endl;
    cout << "Lack_Tri " << lack_Trigger_Num << endl;
  };
#endif

#ifdef PREFETCH_FILTER
  PF_Filter* pf_filter = new PF_Filter(PF_FILTER_SIZE, PF_FILTER_ASSOC);
#endif

  std::string ExtractWorkloadName(const std::string& full_path)
  {
    // input: /path/to/benchmark/tracefile
    // output: workload name
    size_t last_slash = full_path.find_last_of('/');
    if (last_slash == std::string::npos) {
      return "";
    }

    std::string dir_part = full_path.substr(0, last_slash);
    std::string file_part = full_path.substr(last_slash + 1);

    size_t second_last_slash = dir_part.find_last_of('/');
    std::string trace_dir = (second_last_slash == std::string::npos) ? dir_part : dir_part.substr(second_last_slash + 1);

    const std::string prefix = "traces-";
    std::string suite;
    if (trace_dir.substr(0, prefix.size()) == prefix) {
      suite = trace_dir.substr(prefix.size());
    }

    size_t last_dot = file_part.rfind('.');
    if (last_dot == std::string::npos) {
      return "";
    }
    size_t second_last_dot = file_part.rfind('.', last_dot - 1);
    std::string base_name = (second_last_dot == std::string::npos) ? file_part.substr(0, last_dot) : file_part.substr(0, second_last_dot);

    return base_name;
  }
  void set_llc_reference(CACHE* llc)
  {
    llc_cache = llc;
    mainMetaTable->setpp(this);
    llc_cache->set_available_ways(waysForCache);
    cout << "Alloc " << waysForCache << " ways for cache" << endl;

#ifdef ELABORATE_LOG
    log_file_name = "./" + ExtractWorkloadName(champsim::global_trace_name) + ".txt";
    cout << log_file_name << endl;
    logfile.open(log_file_name);
#endif
  }

  int issue_mainMetatable(uint64_t block_addr, int degree);

  /* Functions for miss classification */
  uint64_t get_last_addr(uint64_t pc)
  {
    auto entry = pcTable->find(pc);
    if (entry) {
      return entry->data.addrHistory.front();
    } else {
      return 0;
    }
  };

  std::set<uint64_t> get_triggers(uint64_t target)
  {
    if (mainMetaTable->reverse_metatable.find(target) != mainMetaTable->reverse_metatable.end()) {
      return mainMetaTable->reverse_metatable[target];
    } else {
      return std::set<uint64_t>();
    }
  };

  using champsim::modules::prefetcher::prefetcher;
  void prefetcher_initialize()
  {
    /*
      --- init in function set_llc_reference! ---
    */
  }
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in, std::string latepf);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in,
                                 champsim::address ip);
  void prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};

#endif
