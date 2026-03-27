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
#define PCQ_SIZE 8
#define PC_META_TABLE_SIZE 4096 * 1 * 12
#define PC_META_TABLE_ASSOC 12

// #define CONFLICT_PREFETCHING
#define CONFLICT_META_TABLE_SIZE 4096 * 1 * 12
#define CONFLICT_META_TABLE_ASSOC 12

#define METADATA_RESIZE
#define INIT_RESIZE_WINDOW 100000
#define RUNTIME_RESIZE_WINDOW 10000000

#define DYNAMIC_DEGREE

// regular components
#define DEFAULT_LOOKAHEAD 3
#define DEFAULT_DEGREE 3

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16
#define PC_TABLE_TAG_WIDTH 12

#define INIT_WAY_MARKOV 8
#define MIN_WAY_MARKOV 2
#define MAX_WAY_MARKOV 8
#define META_TABLE_SIZE (4096 * 12 * INIT_WAY_MARKOV)
#define META_TABLE_ASSOC 12

#define PREFETCH_FILTER
#define PF_FILTER_SIZE 512
#define PF_FILTER_ASSOC 8
#define PF_FILTER_TAG_WIDTH 10

class prism;

uint64_t hash_xor(uint64_t key);
uint8_t get_dram_bw();

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

struct pf_filter_entry
{
  uint64_t pf_addr;
  bool fill_l2;
  pf_filter_entry* next;
  pf_filter_entry(uint64_t addr = 0, bool fill = true, pf_filter_entry* ptr = nullptr) : pf_addr(addr), fill_l2(fill), next(ptr){};
};

class PF_Filter : public FIFOSetAssociativeCache<pf_filter_entry>
{
  typedef FIFOSetAssociativeCache<pf_filter_entry> Super;

public:
  const uint64_t mask;
  PF_Filter(int size, int num_ways, int debug_level = 0)
      : Super(size, num_ways, debug_level), mask((1ULL << (__builtin_ctz(size) + PF_FILTER_TAG_WIDTH)) - 1) {};

  pf_filter_entry* insert(uint64_t pf_addr, bool fill_l2)
  {
    uint64_t key = build_key(pf_addr);
    Super::insert(key, {pf_addr, fill_l2});
    return &(Super::find(key)->data);
  }

  pf_filter_entry* find(uint64_t addr)
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
    auto victim = Super::erase(key);
    if (victim){
      victim->data.next = nullptr;
    }
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

  int waysForCache = 16 - INIT_WAY_MARKOV;
  int waysForMarkov = INIT_WAY_MARKOV;
  int origin_waysForMarkov = INIT_WAY_MARKOV;
  bool large_markov = (INIT_WAY_MARKOV == 8) ? 1 : 0;
  int waysForPCTable = 0;
  int origin_waysForPCTable = 0;

  bool init_resized = false;
  uint64_t demand = 0;

  int discard_prefetch = 0;
  int discard_metadata = 0;

  bool warmup_complete = false;
  std::string log_file_name;
  std::ofstream logfile;

  struct PCTableEntry {
    uint64_t lookahead;
    int degree;
    uint64_t latePrefetchCount;
    uint64_t usefulPrefetchCount;
    uint64_t issuedPrefetchCount;
    uint64_t missCount;
    bool modified;
    deque<uint64_t> addrHistory;

    PCTableEntry(uint64_t _last_addr = 0, bool cache_hit = false)
        : lookahead(DEFAULT_LOOKAHEAD), degree(DEFAULT_DEGREE), latePrefetchCount(0), usefulPrefetchCount(0), issuedPrefetchCount(0),
          missCount(cache_hit ? 0 : 1), modified(false)
    {
      addrHistory.push_front(_last_addr);
    };
    void update_counter(uint64_t pc)
    {
      float laterate = 0;
      float accuracy = 0;
      if (latePrefetchCount + usefulPrefetchCount)
        laterate = 1.0 * latePrefetchCount / (latePrefetchCount + usefulPrefetchCount);
      if ((latePrefetchCount + issuedPrefetchCount) != 0)
        accuracy = 1.0 * (latePrefetchCount + usefulPrefetchCount) / (latePrefetchCount + issuedPrefetchCount);

      int delta_degree = 0;
      if (accuracy > 0.75) {
        if (laterate > 0.1) {
          delta_degree = 2;
        } else {
          delta_degree = 1;
        }
      } else if (accuracy < 0.05) {
        delta_degree = -degree;
      } else if (accuracy < 0.15) {
        if (degree > 1)
          delta_degree = -1;
      } else {
        if (laterate > 0.1)
          delta_degree = 1;
      }
      degree += delta_degree;
      if (degree < 0)
        degree = 0;
      if (degree > 6)
        degree = 6;

      latePrefetchCount = 0;
      usefulPrefetchCount = 0;
      issuedPrefetchCount = 0;
      missCount = 0;
    };
  };

  LRUSetAssociativeCache<PCTableEntry>* pcTable = new LRUSetAssociativeCache<PCTableEntry>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  prismMetaTable* mainMetaTable = new prismMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  PF_Filter* pf_filter = new PF_Filter(PF_FILTER_SIZE, PF_FILTER_ASSOC);

  // stat for main metadata table
  uint64_t main_table_lookups = 0;
  uint64_t main_table_hits = 0;
  uint64_t main_table_issued_prefetches = 0;
  uint64_t main_table_accurate_prefetches = 0;
  std::set<uint64_t> main_table_prefetches;

  // For PC_TRIGGER_PREFETCHING
  bool enable_PC_Trigger_prefetching = false;
  int init_ways_for_pc_metadata_table;
  uint64_t unmodified_PC = 0;
  uint64_t inserted_PC = 0;
  std::deque<uint64_t> PCQ;
  // SRRIPSetAssociativeCache<MetaTableEntry>* pcMetaTable = new SRRIPSetAssociativeCache<MetaTableEntry>(PC_META_TABLE_SIZE, PC_META_TABLE_ASSOC);
  SRRIPSetAssociativeCache<MetaTableEntry>* pcMetaTable = nullptr;
  // stat for pc metadata table
  uint64_t PCM_late_prefetches = 0;
  uint64_t PCM_useful_prefetches = 0;
  uint64_t PCM_useless_prefetches = 0;
  std::set<uint64_t> PCM_issued_prefetches;
  std::set<uint64_t> PCM_filled_prefetches;

  void update_pc_metatable_size()
  {
    // if (!init_resized) {
    if (inserted_PC >= 0.25 * INIT_RESIZE_WINDOW) {
      enable_PC_Trigger_prefetching = true;
      waysForPCTable = 2;
    } else if (inserted_PC >= 0.05 * INIT_RESIZE_WINDOW) {
      enable_PC_Trigger_prefetching = true;
      waysForPCTable = 1;
    } else {
      enable_PC_Trigger_prefetching = false;
      waysForPCTable = 0;
    }
    // }
    // else {
    //   if (waysForPCTable < 2 && inserted_PC >= 4096 * 12 * 2) {
    //     enable_PC_Trigger_prefetching = true;
    //     waysForPCTable = 2;
    //   } else if (waysForPCTable < 1 && inserted_PC >= 4096 * 12 * 1) {
    //     enable_PC_Trigger_prefetching = true;
    //     waysForPCTable = 1;
    //   }
    // }

    if (waysForPCTable > 0 && waysForPCTable != origin_waysForPCTable) {
      SRRIPSetAssociativeCache<MetaTableEntry>* resized_pcMetaTable =
          new SRRIPSetAssociativeCache<MetaTableEntry>(4096 * 12 * waysForPCTable, PC_META_TABLE_ASSOC);
      if (pcMetaTable) {
        for (int i = 0; i < pcMetaTable->num_sets; i++) {
          if (i % origin_waysForPCTable < waysForPCTable) {
            for (int j = 0; j < pcMetaTable->entries[i].size(); j++) {
              if (pcMetaTable->entries[i][j].valid) {
                resized_pcMetaTable->insert(pcMetaTable->entries[i][j].key, pcMetaTable->entries[i][j].data);
                resized_pcMetaTable->set_rrpv(pcMetaTable->entries[i][j].key, pcMetaTable->rrpv[i][j]);
              }
            }
          }
        }
        delete pcMetaTable;
      }
      pcMetaTable = resized_pcMetaTable;
      cout << "Resize PC metadata table. Alloc " << waysForPCTable << " ways for PC metadata!" << endl;
    }

    if (!init_resized) {
      init_ways_for_pc_metadata_table = waysForPCTable;
    }

    origin_waysForPCTable = waysForPCTable;
    inserted_PC = 0;
    unmodified_PC = 0;
  }

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

  // for main metadata resize
  uint64_t resize_useful_prefetch = 0;
  uint64_t resize_issued_prefetch = 0;
  long last_llc_hits = 0;
  long last_llc_misses = 0;
  float init_resize_score, init_resize_llc_hit_rate, init_resize_useful_prefetch_rate;
  int init_ways_for_markov;
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
      temp_resize_score = 3 * temp_llc_hit_rate - 1 * temp_useful_prefetch_rate;
      if (temp_resize_score > 0) {
        waysForCache = 16 - MIN_WAY_MARKOV;
        waysForMarkov = MIN_WAY_MARKOV;
        large_markov = false;
      }
    } else {
      temp_resize_score = 1.5 * temp_llc_hit_rate - 1 * temp_useful_prefetch_rate;
      if (temp_resize_score < 0) {
        waysForCache = 16 - MAX_WAY_MARKOV;
        waysForMarkov = MAX_WAY_MARKOV;
        large_markov = true;
      }
    }

    // for stat, only record the init resize data
    if (!init_resized) {
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
    if ((waysForCache + waysForMarkov + waysForPCTable) != 16) {
      int delta = waysForCache + waysForMarkov + waysForPCTable - 16;
      if (waysForMarkov > delta) {
        waysForMarkov -= delta;
      } else {
        waysForMarkov = 1;
        large_markov = false;
        waysForCache = 16 - waysForMarkov - waysForPCTable;
      }
    }

    llc_cache->set_available_ways(waysForCache);

    if (waysForMarkov != origin_waysForMarkov) {
      // resize markov table
      prismMetaTable* resized_metadata_table = new prismMetaTable(4096 * META_TABLE_ASSOC * waysForMarkov, META_TABLE_ASSOC);
      for (int i = 0; i < mainMetaTable->num_sets; i++) {

        if (i % origin_waysForMarkov < waysForMarkov) {
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
      mainMetaTable->setpp(this);
      cout << "Resize markov table. Alloc " << waysForMarkov << " ways for metadata!" << endl;
    }

    if (!init_resized) {
      init_ways_for_markov = waysForMarkov;
    }

    origin_waysForMarkov = waysForMarkov;
  }

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

  int issue_mainMetatable(uint64_t pc, uint64_t block_addr, int degree);

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
