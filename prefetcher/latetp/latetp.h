#ifndef LATETP
#define LATETP

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

// #define DYNAMIC_GLOBAL_DEGREE
#define DYNAMIC_LOCAL_DEGREE
#define BW_DEGREE
#define DEFAULT_LOOKAHEAD 4
#define DEFAULT_DEGREE 2

#define FILTER_MODE 0 // 0: no filter; 1: ideal; 2: directly map table
#define PF_FILTER_SIZE 512
#define PF_FILTER_TAG_WIDTH 8

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16

#define WAY_MARKOV 4
#define META_TABLE_SIZE (4096 * 12 * WAY_MARKOV)
#define META_TABLE_ASSOC 12

class latetp;

uint8_t get_dram_bw();

struct latetpMetaTableEntry {

  uint64_t correlated_addr;
  bool used;

  latetpMetaTableEntry() : correlated_addr(0), used(false) {};
  latetpMetaTableEntry(uint64_t addr) : correlated_addr(addr) {};
};

class latetpMetaTable : public LRUSetAssociativeCache<latetpMetaTableEntry>
{
  typedef LRUSetAssociativeCache<latetpMetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  latetp* prefetcher;

  latetpMetaTable(int size, int num_ways) : Super(size, num_ways), prefetcher(nullptr) {}

  void setpp(latetp* p) { prefetcher = p; }

  latetpMetaTableEntry* find(uint64_t key)
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
  bool insert(uint64_t key, const latetpMetaTableEntry& data);

  Entry* erase(uint64_t key) { return Super::erase(key); }
};

class PF_Filter
{
public:
  uint64_t size;
  uint8_t width;
  vector<uint64_t> table;
  PF_Filter(int size, int width) : size(size), width(width) { table.resize(size, 0); }

  bool add(uint64_t addr)
  {
    uint64_t index = addr % size;
    uint64_t key = build_key(addr);
    if (table[index] == key) {
      return true;
    } else {
      table[index] = key;
      return false;
    }
  }

  bool erase(uint64_t addr)
  {
    uint64_t index = addr % size;
    if (table[index] == build_key(addr)) {
      table[index] = 0;
      return true;
    } else {
      return false;
    }
  }

  bool find(uint64_t addr)
  {
    uint64_t index = addr % size;
    if (table[index] == build_key(addr)) {
      return true;
    } else {
      return false;
    }
  }

  uint64_t build_key(uint64_t addr)
  {
    const uint64_t MASK = (1ULL << width) - 1;
    uint64_t hash = 0;
    while (addr != 0) {
      hash ^= (addr & MASK);
      addr >>= width;
    }
    return hash;
  }
};

class latetp : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;

  std::string benchmark;

  uint32_t numEntriesinTable = 0;
  int waysForCache = 16 - WAY_MARKOV;

  // stat
  uint64_t meta_table_lookups = 0;
  uint64_t meta_table_hits = 0;
  uint64_t meta_table_issued_prefetches = 0;
  uint64_t meta_table_accurate_prefetches = 0;
  std::set<uint64_t> meta_table_prefetches;

  latetpMetaTable* metaTable = new latetpMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  struct PCTableEntry {
    int lookahead;
    int degree;
    uint64_t latePrefetchCount;
    uint64_t usefulPrefetchCount;
    uint64_t issuedPrefetchCount;
    uint64_t filledPrefetchCount;
    uint64_t missCount;
    deque<uint64_t> addrHistory;

    PCTableEntry(uint64_t _lastAddr = 0)
        : lookahead(DEFAULT_LOOKAHEAD), degree(DEFAULT_DEGREE), latePrefetchCount(0), usefulPrefetchCount(0), issuedPrefetchCount(0), filledPrefetchCount(0),
          missCount(0) {
            // addrHistory.push_front(_lastAddr);
          };
    void update_counter()
    {
      float laterate = 1.0 * latePrefetchCount / (latePrefetchCount + usefulPrefetchCount);
      float accuracy = 1.0 * (latePrefetchCount + usefulPrefetchCount) / (latePrefetchCount + filledPrefetchCount);

      int delta_degree = 0;
      if (accuracy > 0.75) {
        if (laterate > 0.1) {
          delta_degree = 2;
        } else {
          delta_degree = 1;
        }
      } else if (accuracy > 0.5) {
        if (laterate > 0.1)
          delta_degree = 1;
      } else if (accuracy < 0.375) {
        delta_degree -= 1;
      }

      degree += delta_degree;
      if (degree < 1)
        degree = 1;
      else if (degree > 4)
        degree = 4;

      latePrefetchCount = 0;
      usefulPrefetchCount = 0;
      issuedPrefetchCount = 0;
      filledPrefetchCount = 0;
      missCount = 0;
    };
  };

  LRUSetAssociativeCache<PCTableEntry>* pcTable = new LRUSetAssociativeCache<PCTableEntry>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  uint64_t global_degree = DEFAULT_DEGREE;
  uint64_t late_prefetch_num = 0;
  uint64_t accurate_prefetch_num = 0;
  uint64_t useless_prefetch_num = 0;
  uint64_t epoch_demand = 0;
  std::set<uint64_t> unused_prefetches;
#ifdef DYNAMIC_GLOBAL_DEGREE
  void tune_global_degree()
  {
    float accuracy = 1.0 * accurate_prefetch_num / (accurate_prefetch_num + useless_prefetch_num);
    float laterate = 1.0 * late_prefetch_num / accurate_prefetch_num;
    int delta_degree = 0;
    if (accuracy > 0.75) {
      if (laterate > 0.125) {
        delta_degree = 2;
      } else {
        delta_degree = 1;
      }
    } else if (accuracy > 0.5) {
      if (laterate > 0.125)
        delta_degree = 1;
    } else if (accuracy < 0.25) {
      delta_degree -= 1;
    }

    global_degree += delta_degree;
    if (global_degree < 1)
      global_degree = 1;
    else if (global_degree > 4)
      global_degree = 4;

    late_prefetch_num = 0;
    accurate_prefetch_num = 0;
    useless_prefetch_num = 0;
    epoch_demand = 0;
  };
#endif

#if FILTER_MODE == 1
  std::set<uint64_t> pf_filter;
#elif FILTER_MODE == 2
  PF_Filter pf_filter = PF_Filter(PF_FILTER_SIZE, PF_FILTER_TAG_WIDTH);
#endif

  std::string log_file_name;
  std::string hint_file;
  std::ofstream logfile;
  bool warmup_complete = false;

  std::string toProfilePath(const std::string& full_path)
  {
    size_t last_slash = full_path.find_last_of('/');
    if (last_slash == std::string::npos) {
      return "";
    }
    // 1. 找到最后一个 '/' 的位置，分离目录和文件名
    std::string dir_part = full_path.substr(0, last_slash);
    std::string file_part = full_path.substr(last_slash + 1);
    // 2. 从 dir_part 中提取最后一级目录名（即 traces-spec2017）
    size_t second_last_slash = dir_part.find_last_of('/');
    std::string trace_dir = (second_last_slash == std::string::npos) ? dir_part : dir_part.substr(second_last_slash + 1);
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
    std::string base_name = (second_last_dot == std::string::npos) ? file_part.substr(0, last_dot) : file_part.substr(0, second_last_dot);
    return base_name;
  }

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

  int issue_metatable(latetpMetaTable* metaTable, uint64_t lookup, uint64_t degree, std::vector<uint64_t>& addresses);

  uint64_t get_last(uint64_t ip)
  {
    auto pc_entry = pcTable->find(ip);
    if (pc_entry) {
      return pc_entry->data.addrHistory.front();
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
#ifdef ELABORATE_LOG
    benchmark = champsim::global_trace_name;
    log_file_name = "./" + toProfilePath(benchmark) + ".txt";
    cout << log_file_name << endl;
    logfile.open(log_file_name);
#endif
  }

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in, std::string latepf);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in,
                                 champsim::address ip);
  void prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};

#endif // __MEM_CACHE_PREFETCH_latetp_HH__
