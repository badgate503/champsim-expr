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

#include "cache.h"
#include "champsim.h"
#include "prism_framework.h"

#ifndef ABLATION_STUDY
#define PC_TRIGGER_PREFETCHING
#define TG_PREFETCHING
#define BMP_RESIZE
#define INSERTION_POLICY
#define REPLACEMENT_POLICY
#endif

// for PC-Triggerd Prefetching
#ifndef PCQ_SIZE
#define PCQ_SIZE 8
#endif
#define MAX_WAY_PAT 2
#define PAT_ASSOC 12
#ifndef PAT_SIZE
#define PAT_SIZE (N_LLC_SET * MAX_WAY_PAT * PAT_ASSOC)
#endif

// for Timeliness-Guaranteed Prefetching
#ifdef TG_PREFETCHING
#define DEFAULT_LOOKAHEAD 2
#define DEFAULT_DEGREE 3
#define DYNAMIC_DEGREE
#ifndef T_ACC_LOW
#define T_ACC_LOW 0.15
#endif
#ifndef T_ACC_HIGH
#define T_ACC_HIGH 0.75
#endif
#define PREFETCH_FILTER
#define PF_FILTER_SIZE 256
#define PF_FILTER_ASSOC 8
#define PF_FILTER_TAG_WIDTH 10
#else
#define DEFAULT_LOOKAHEAD 0
#define DEFAULT_DEGREE 1
#endif

// for BMP resize
#ifdef BMP_RESIZE
#define INIT_WAY_MARKOV 8
#else
#define INIT_WAY_MARKOV 4
#endif
#define RESIZE_SAMPLE_WINDOW 100000
#define SAMPLE_INTERVAL 10000000
#ifndef K_AGGR
#define K_AGGR 1.5
#endif
#ifndef K_CONS
#define K_CONS 1.5
#endif

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16
#define PC_TABLE_TAG_WIDTH 12

#define MIN_WAY_MARKOV 1
#define MAX_WAY_MARKOV 8
#define META_TABLE_ASSOC 12
#define META_TABLE_SIZE (N_LLC_SET * MAX_WAY_MARKOV * META_TABLE_ASSOC)


class prism;

uint64_t hash_xor(uint64_t key);

struct MetaTableEntry {
public:
  uint64_t target_addr;
  bool confident;
  MetaTableEntry(uint64_t addr = 0) : target_addr(addr), confident(false) {};
};

class prismPCAddressTable : public SRRIPSetAssociativeCache<MetaTableEntry>
{
  typedef SRRIPSetAssociativeCache<MetaTableEntry> Super;

public:
  int alloc_sets;
  int max_sets;

  prismPCAddressTable(int size, int num_ways) : alloc_sets(size / PAT_ASSOC), max_sets(size / PAT_ASSOC), Super(size, num_ways) {}

  void resize(int waysForPAT) { alloc_sets = waysForPAT * N_LLC_SET; }

  MetaTableEntry* find(uint64_t key)
  {
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    Super::touch(key);
    return &(entry->data);
  }

  void insert(uint64_t key, const MetaTableEntry& data)
  {
    Entry entry = Super::insert(key, data);
    if (entry.valid && entry.key != key) {
      Super::set_default(key);
    } else {
      Super::touch(key);
    }
  }

  virtual uint64_t get_index(uint64_t key) { return key % this->alloc_sets; }
  virtual uint64_t get_tag(uint64_t key) { return key / this->max_sets; }
};

#ifdef REPLACEMENT_POLICY
class prismMetaTable : public SHIPSetAssociativeCache<MetaTableEntry>
{
  typedef SHIPSetAssociativeCache<MetaTableEntry> Super;
#else
class prismMetaTable : public LRUSetAssociativeCache<MetaTableEntry>
{
  typedef LRUSetAssociativeCache<MetaTableEntry> Super;
#endif
public:
  int alloc_sets;
  int max_sets;

  prismMetaTable(int size, int num_ways) : alloc_sets(INIT_WAY_MARKOV * N_LLC_SET), max_sets(MAX_WAY_MARKOV * N_LLC_SET), Super(size, num_ways) {}

  void resize(int waysForMarkov) { alloc_sets = waysForMarkov * N_LLC_SET; }

  MetaTableEntry* find(uint64_t key)
  {
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    return &(entry->data);
  }

  void insert(uint64_t key, uint64_t ip, const MetaTableEntry& data)
  {
    Super::insert(key, data);
#ifdef REPLACEMENT_POLICY
    Super::set_default(key, ip);
#else
    Super::set_mru(key);
#endif
  }

  virtual uint64_t get_index(uint64_t key) { return key % this->alloc_sets; }
  virtual uint64_t get_tag(uint64_t key) { return key / this->max_sets; }
};

struct pf_filter_entry {
  uint64_t cur_addr;
  uint64_t next_addr;
  bool fill_l2;
  pf_filter_entry(uint64_t _addr = 0, uint64_t _next_addr = 0, bool _fill_l2 = true) : cur_addr(_addr), next_addr(_next_addr), fill_l2(_fill_l2) {};
};

class PF_Filter : public FIFOSetAssociativeCache<pf_filter_entry>
{
  typedef FIFOSetAssociativeCache<pf_filter_entry> Super;

public:
  const uint64_t mask;

  PF_Filter(int size, int num_ways, int debug_level = 0)
      : Super(size, num_ways, debug_level), mask((1ULL << (__builtin_ctz(size) + PF_FILTER_TAG_WIDTH)) - 1) {};

  pf_filter_entry* insert(uint64_t cur_addr, uint64_t next_addr, bool fill_l2 = true)
  {
    uint64_t key = build_key(cur_addr);
    Super::insert(key, {cur_addr, next_addr, fill_l2});
    return &(Super::find(key)->data);
  }

  pf_filter_entry* find(uint64_t cur_addr)
  {
    uint64_t key = build_key(cur_addr);
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    return &(entry->data);
  };

  // may broke FIFO
  void erase(uint64_t cur_addr)
  {
    uint64_t key = build_key(cur_addr);
    auto victim = Super::erase(key);
  }

  uint64_t build_key(uint64_t cur_addr)
  {
    uint64_t key = hash_xor(cur_addr);
    key &= mask;
    return key;
  };
};

class prism : public champsim::modules::prefetcher
{
public:
  CACHE* llc_cache = NULL;
  int debug_level = 0;

  int waysForCache = 16 - INIT_WAY_MARKOV;
  int waysForMarkov = INIT_WAY_MARKOV;
  bool large_markov = (INIT_WAY_MARKOV == MAX_WAY_MARKOV) ? 1 : 0;
  int waysForPAT = 0;
  int origin_waysForPAT = 0;
  bool init_resized = false;
  uint64_t demand = 0;

  int discard_prefetch = 0;
  int discard_metadata = 0;

  bool warmup_complete = false;

  // stat MT lookups
  uint64_t MT_lookups = 0;
  uint64_t MT_hits = 0;
  uint64_t MT_inserts = 0;
  uint64_t PAT_lookups = 0;
  uint64_t PAT_hits = 0;
  uint64_t PAT_inserts = 0;
  bool warmup_reset = false;

  struct energy_stat {
    uint64_t training_table_read;
    uint64_t training_table_write;
    uint64_t filter_table_read;
    uint64_t filter_table_write;
    uint64_t markov_read[2];
    uint64_t markov_write[2];
    uint64_t pat_read[2];
    uint64_t pat_write[2];
    void reset() { *this = {}; }
  } energy_stats;

  int pat_lookup_times[7] = {0, 0, 0, 0, 0, 0, 0};

  struct PCTableEntry {
    uint64_t lookahead;
    int degree;
    uint64_t usefulPrefetchCount;
    uint64_t issuedPrefetchCount;
    uint64_t hitCount;
    bool modified;
    deque<uint64_t> addrHistory;

    PCTableEntry(uint64_t _last_addr = 0, bool cache_hit = false)
        : lookahead(DEFAULT_LOOKAHEAD), degree(DEFAULT_DEGREE), usefulPrefetchCount(0), issuedPrefetchCount(0), hitCount(cache_hit ? 1 : 0), modified(false)
    {
      addrHistory.push_front(_last_addr);
    };
    void update_counter(uint64_t pc)
    {
#ifdef DYNAMIC_DEGREE
      float accuracy = 0;
      //  Here we ignore late prefetches when calculating accuracy,
      //  since tracking late prefetches incurs additional overhead
      if (issuedPrefetchCount != 0)
        accuracy = 1.0 * usefulPrefetchCount / issuedPrefetchCount;

      int delta_degree = 0;

      if (accuracy <= T_ACC_LOW) {
        delta_degree = -1;
      } else if (accuracy > T_ACC_HIGH) {
        delta_degree += 1;
      }

      degree += delta_degree;
      if (degree < 0)
        degree = 0;
      if (degree > 6)
        degree = 6;
#endif

      usefulPrefetchCount = 0;
      issuedPrefetchCount = 0;
    };
  };

  LRUSetAssociativeCache<PCTableEntry>* pcTable = new LRUSetAssociativeCache<PCTableEntry>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  prismMetaTable* mainMetaTable = new prismMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  PF_Filter* pf_filter = new PF_Filter(PF_FILTER_SIZE, PF_FILTER_ASSOC);

  // stat for main metadata table
  uint64_t main_table_issued_prefetches = 0;
  uint64_t main_table_accurate_prefetches = 0;
  std::set<uint64_t> main_table_prefetches;

  // For PC_TRIGGER_PREFETCHING
#ifndef INDEPENDENT_PAT
  bool enable_PC_Trigger_prefetching = false;
#else
  bool enable_PC_Trigger_prefetching = true;
#endif
  int init_ways_for_pc_metadata_table;
  uint64_t fail_entry_init = 0;
  uint64_t inserted_PC = 0;
  uint64_t init_insert_PC = 0;
  uint64_t init_fail_entry_init = 0;
  std::deque<uint64_t> PCQ;
#ifdef INF_PAT
  unordered_map<uint64_t, MetaTableEntry> pcMetaTable;
#else
  prismPCAddressTable* pcMetaTable = new prismPCAddressTable(PAT_SIZE, PAT_ASSOC);
#endif
  // stat for pc metadata table
  uint64_t PCM_late_prefetches = 0;
  uint64_t PCM_useful_prefetches = 0;
  uint64_t PCM_useless_prefetches = 0;
  std::set<uint64_t> PCM_issued_prefetches;
  std::set<uint64_t> PCM_filled_prefetches;

  void update_pc_metatable_size()
  {
#ifndef INDEPENDENT_PAT
    if (fail_entry_init >= 0.3 * RESIZE_SAMPLE_WINDOW) {
      enable_PC_Trigger_prefetching = true;
      waysForPAT = 2;
    } else if (fail_entry_init >= 0.15 * RESIZE_SAMPLE_WINDOW) {
      enable_PC_Trigger_prefetching = true;
      waysForPAT = 1;
    } else {
      enable_PC_Trigger_prefetching = false;
      waysForPAT = 0;
    }

    pcMetaTable->resize(waysForPAT);
    if (waysForPAT != origin_waysForPAT)
      cout << "Resize PC metadata table. Alloc " << waysForPAT << " ways for PC metadata!" << endl;

    // for stat, only record the init resize data
    if (!init_resized) {
      init_ways_for_pc_metadata_table = waysForPAT;
      init_insert_PC = inserted_PC;
      init_fail_entry_init = fail_entry_init;
    }

    origin_waysForPAT = waysForPAT;
    inserted_PC = 0;
    fail_entry_init = 0;
#endif
  }

  // for main metadata resize
  uint64_t resize_useful_prefetch = 0;
  uint64_t resize_issued_prefetch = 0;
  long last_llc_hits = 0;
  long last_llc_misses = 0;
  float init_resize_score, init_resize_llc_hit_rate, init_resize_useful_prefetch_rate;
  int init_ways_for_markov;
  bool sample_start = false;
  bool init_sample = false;
  void reset_for_sample()
  {
    using hits_value_type = typename decltype(llc_cache->sim_stats.hits)::value_type;
    using misses_value_type = typename decltype(llc_cache->sim_stats.misses)::value_type;
    resize_useful_prefetch = 0;
    resize_issued_prefetch = 0;
    last_llc_hits = llc_cache->sim_stats.hits.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, hits_value_type{});
    last_llc_misses = llc_cache->sim_stats.misses.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, misses_value_type{});
    demand = 0;
  }
  void update_main_metatable_size()
  {
    using hits_value_type = typename decltype(llc_cache->sim_stats.hits)::value_type;
    using misses_value_type = typename decltype(llc_cache->sim_stats.misses)::value_type;
    float temp_llc_hit_rate = 0;
    float temp_useful_prefetch_rate = 0;
    float temp_resize_score = 0;

    long llc_hits = (llc_cache->sim_stats.hits.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, hits_value_type{})) - last_llc_hits;
    long llc_misses = (llc_cache->sim_stats.misses.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, misses_value_type{})) - last_llc_misses;

    if (llc_hits)
      temp_llc_hit_rate = (1.0 * llc_hits / (llc_hits + llc_misses));
    if (resize_issued_prefetch)
      temp_useful_prefetch_rate = (1.0 * resize_useful_prefetch / resize_issued_prefetch);

    if (large_markov) {
      temp_resize_score = K_AGGR * temp_llc_hit_rate - temp_useful_prefetch_rate;
      if (temp_resize_score > 0) {
        waysForCache = 16 - MIN_WAY_MARKOV;
        waysForMarkov = MIN_WAY_MARKOV;
        large_markov = false;
        cout << "Resize markov table. Alloc " << waysForMarkov << " ways for metadata!" << endl;
      } else {
        cout << "Donot Resize markov table. Alloc " << waysForMarkov << " ways for metadata!" << endl;
      }
    } else {
      temp_resize_score = K_CONS * temp_llc_hit_rate - temp_useful_prefetch_rate;
      if (temp_resize_score < 0) {
        waysForCache = 16 - MAX_WAY_MARKOV;
        waysForMarkov = MAX_WAY_MARKOV;
        large_markov = true;
        cout << "Resize markov table. Alloc " << waysForMarkov << " ways for metadata!" << endl;
      } else {
        cout << "Donot Resize markov table. Alloc " << waysForMarkov << " ways for metadata!" << endl;
      }
    }

    // resize the main metadata table
    mainMetaTable->resize(waysForMarkov);

    // for stat, only record the init resize data
    if (!init_resized) {
      init_ways_for_markov = waysForMarkov;
      init_resize_score = temp_resize_score;
      init_resize_llc_hit_rate = temp_llc_hit_rate;
      init_resize_useful_prefetch_rate = temp_useful_prefetch_rate;
    }

    resize_useful_prefetch = 0;
    resize_issued_prefetch = 0;
    last_llc_hits = llc_cache->sim_stats.hits.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, hits_value_type{});
    last_llc_misses = llc_cache->sim_stats.misses.value_or(std::pair{access_type::LOAD, llc_cache->cpu}, misses_value_type{});
  };

  void resize_cache()
  {
    waysForCache = 16 - waysForMarkov - waysForPAT;
    llc_cache->set_available_ways(waysForCache);

    if (!init_resized) {
      init_resized = true;
    }
  }

  void reset_stat_counters()
  {
    MT_lookups = 0;
    MT_hits = 0;
    MT_inserts = 0;
    PAT_lookups = 0;
    PAT_hits = 0;
    PAT_inserts = 0;

    main_table_issued_prefetches = 0;
    main_table_accurate_prefetches = 0;

    PCM_late_prefetches = 0;
    PCM_useful_prefetches = 0;
    PCM_useless_prefetches = 0;

    inserted_PC = 0;
    fail_entry_init = 0;

    energy_stats.reset();
    memset(pat_lookup_times, 0, sizeof(pat_lookup_times));

    warmup_reset = true;
  }

  void set_llc_reference(CACHE* llc)
  {
    llc_cache = llc;
    llc_cache->set_available_ways(waysForCache);
    cout << "Alloc " << waysForCache << " ways for cache" << endl;
  }

  int issue_mainMetatable(uint64_t pc, uint64_t block_addr, int degree);

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
