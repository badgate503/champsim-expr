#ifndef PCTP
#define PCTP

/*
  This prefetcher is: baseline + PC Triggered Prefetching (PCTP). 
  We use this prefetcher to evaluate the sensitivity of PCTP parameters.
*/

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

#ifndef PCQ_SIZE
#define PCQ_SIZE 8
#endif

#ifndef PAT_SIZE
#define PAT_SIZE (4 * 1024 * 1 * 12)
#endif
#define PAT_ASSOC 12

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16

#define WAY_MARKOV 4
#define META_TABLE_ASSOC 12
#define META_TABLE_SIZE (4096 * 12 * WAY_MARKOV)
#define GLOBAL_DEGREE 1 

class pctp;

uint64_t hash_xor(uint64_t addr);

struct pctpMetaTableEntry {

  uint64_t correlated_addr;
  bool used;

  pctpMetaTableEntry() : correlated_addr(0), used(false) {};
  pctpMetaTableEntry(uint64_t addr) : correlated_addr(addr) {};
};

class pctpMetaTable : public LRUSetAssociativeCache<pctpMetaTableEntry>
{
  typedef LRUSetAssociativeCache<pctpMetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  pctp* prefetcher;

  pctpMetaTable(int size, int num_ways) : Super(size, num_ways), prefetcher(nullptr) {}

  void setpp(pctp* p) { prefetcher = p; }

  pctpMetaTableEntry* find(uint64_t key)
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
  bool insert(uint64_t key, const pctpMetaTableEntry& data);

  Entry* erase(uint64_t key) { return Super::erase(key); }
};

class pctp : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;
  int globalDegree = GLOBAL_DEGREE;

  std::string benchmark;

  uint32_t numEntriesinTable = 0;
  int waysForCache = 16 - WAY_MARKOV;

  // stat
  uint64_t meta_table_lookups = 0;
  uint64_t meta_table_hits = 0;
  uint64_t meta_table_issued_prefetches = 0;
  uint64_t meta_table_accurate_prefetches = 0;
  std::set<uint64_t> meta_table_prefetches;

  pctpMetaTable* metaTable = new pctpMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  LRUSetAssociativeCache<std::deque<uint64_t>>* pcTable = new LRUSetAssociativeCache<std::deque<uint64_t>>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  std::deque<uint64_t> PCQ;
#ifdef INF_PAT
  std::map<uint64_t, uint64_t> pc_meta_table; // <trigger pc, block_addr>
#else
  SRRIPSetAssociativeCache<uint64_t>* pc_meta_table = new SRRIPSetAssociativeCache<uint64_t>(PAT_SIZE, PAT_ASSOC);
#endif
  // stat
  std::set<uint64_t> PCM_issued_prefetches, PCM_filled_prefetches;
  uint64_t PCM_late_prefetches = 0, PCM_useful_prefetches = 0, PCM_useless_prefetches = 0;

  std::string log_file_name;
  std::string hint_file;
  std::ofstream logfile;
  bool warmup_complete = false;

  void set_llc_reference(CACHE* llc)
  {
    llc_cache = llc;
    llc_cache->set_available_ways(waysForCache);
  }

  bool isAlreadyInQueue(std::vector<uint64_t>& addresses, uint64_t addr)
  {
    for (uint64_t& a : addresses) {
      if (a == addr)
        return true;
    }
    return false;
  }

  int issue_metatable(pctpMetaTable* metaTable, uint64_t lookup, std::vector<uint64_t>& addresses);

  void invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t>& pref_addr);

  using champsim::modules::prefetcher::prefetcher;

  void prefetcher_initialize()
  {
    metaTable->setpp(this);
  }

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in, std::string latepf);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};

#endif // __MEM_CACHE_PREFETCH_pctp_HH__