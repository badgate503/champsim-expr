#ifndef BASELINE
#define BASELINE

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "prism_framework.h"
#include "cache.h"
#include "champsim.h"

// #define MISS_CLASS_LOG

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16
#define WAY_MARKOV 4
#define META_TABLE_SIZE (N_LLC_SET * 12 * WAY_MARKOV)
#define META_TABLE_ASSOC 12
#define GLOBAL_DEGREE 1 

class baseline;

struct baselineMetaTableEntry {

  uint64_t correlated_addr;
  bool used;

  baselineMetaTableEntry() : correlated_addr(0), used(false) {};
  baselineMetaTableEntry(uint64_t addr) : correlated_addr(addr) {};
};

class baselineMetaTable : public LRUSetAssociativeCache<baselineMetaTableEntry>
{
  typedef LRUSetAssociativeCache<baselineMetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  baseline* prefetcher;

  baselineMetaTable(int size, int num_ways) : Super(size, num_ways), prefetcher(nullptr) {}

  void setpp(baseline* p) { prefetcher = p; }

  baselineMetaTableEntry* find(uint64_t key)
  {
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    return &(entry->data);
  }

  /*
      If evict another valid entry: return true!
      else: return false!
  */
  bool insert(uint64_t key, const baselineMetaTableEntry& data);

  Entry* erase(uint64_t key) { return Super::erase(key); }
};

class baseline : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;
  int globalDegree = GLOBAL_DEGREE;

  std::string benchmark;
  uint32_t numEntriesinTable = 0;

  // stat
  uint64_t meta_table_issued_prefetches = 0;
  uint64_t meta_table_accurate_prefetches = 0;
  std::set<uint64_t> meta_table_prefetches;

  baselineMetaTable* metaTable = new baselineMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  LRUSetAssociativeCache<std::deque<uint64_t>>* pcTable = new LRUSetAssociativeCache<std::deque<uint64_t>>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  std::string log_file_name;
  std::string hint_file;
  std::ofstream logfile;
  bool warmup_complete = false;

  // stat MT lookups
  uint64_t MT_lookups = 0;
  uint64_t MT_hits = 0;
  uint64_t MT_inserts = 0;
  // uint64_t MT_lookup_reqs = 0;
  // uint64_t MT_lookup_returns = 0;
  bool warmup_reset = false;

  uint64_t training_unit_read = 0;
  uint64_t training_unit_write = 0;
  uint64_t markov_read = 0;
  uint64_t markov_write = 0;

  void reset_stat_counters()
  {
    MT_lookups = 0;
    MT_hits = 0;
    MT_inserts = 0;

    training_unit_read = 0;
    training_unit_write = 0;
    markov_read = 0;
    markov_write = 0;

    meta_table_issued_prefetches = 0;
    meta_table_accurate_prefetches = 0;
    warmup_reset = true;
  }

  std::string getTraceName(const std::string& full_path)
  {
    // this function is to extract trace name from the input trace path
    size_t last_slash = full_path.find_last_of('/');
    if (last_slash == std::string::npos) {
      return "";
    }
    // 1. find the last '/'
    std::string dir_part = full_path.substr(0, last_slash);
    std::string file_part = full_path.substr(last_slash + 1);
    // 2.
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
    llc_cache->set_available_ways(16- WAY_MARKOV);
  }

  bool isAlreadyInQueue(std::vector<uint64_t>& addresses, uint64_t addr)
  {
    for (uint64_t& a : addresses) {
      if (a == addr)
        return true;
    }
    return false;
  }

  int issue_metatable(baselineMetaTable* metaTable, uint64_t pc, uint64_t lookup, std::vector<uint64_t>& addresses);

  uint64_t get_last(uint64_t ip)
  {
    auto pc_entry = pcTable->find(ip);
    if (pc_entry) {
      return pc_entry->data.front();
    } else {
      return 0;
    }
  };

  std::set<uint64_t> get_triggers(uint64_t target)
  {
    if (metaTable->reverse_metatable.find(target) != metaTable->reverse_metatable.end()) {
      return metaTable->reverse_metatable[target];
    } else {
      return std::set<uint64_t>();
    }
  };

  using champsim::modules::prefetcher::prefetcher;

  void prefetcher_initialize()
  {
    metaTable->setpp(this);
#ifdef MISS_CLASS_LOG
    benchmark = champsim::global_trace_name;
    log_file_name = "./" + getTraceName(benchmark) + ".txt";
    cout << log_file_name << endl;
    logfile.open(log_file_name);
#endif
  }

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in, std::string latepf);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};

#endif // __MEM_CACHE_PREFETCH_baseline_HH__
