#ifndef RESIZE
#define RESIZE

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

#define INIT_WINDOW 100000
#define BASE_TRIGGER_NUM 1
// #define MISS_CLASS_LOG

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16
#define WAY_MARKOV 8
#define META_TABLE_SIZE (4096 * 12 * WAY_MARKOV)
#define META_TABLE_ASSOC 12
#define GLOBAL_DEGREE 1

class resize_pr;

struct resize_prMetaTableEntry {

  uint64_t correlated_addr;
  bool used;

  resize_prMetaTableEntry() : correlated_addr(0), used(false) {};
  resize_prMetaTableEntry(uint64_t addr) : correlated_addr(addr) {};
};

class resize_prMetaTable : public SRRIPSetAssociativeCache<resize_prMetaTableEntry>
{
  typedef SRRIPSetAssociativeCache<resize_prMetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  resize_pr* prefetcher;

  resize_prMetaTable(int size, int num_ways) : Super(size, num_ways), prefetcher(nullptr) {}

  void setpp(resize_pr* p) { prefetcher = p; }

  resize_prMetaTableEntry* find(uint64_t key)
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
  bool insert(uint64_t key, const resize_prMetaTableEntry& data);
  bool insert(uint64_t key, const resize_prMetaTableEntry& data, uint64_t rrpv_value);

  Entry* erase(uint64_t key) { return Super::erase(key); }
};

class resize_pr : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;
  int globalDegree = GLOBAL_DEGREE;

  std::string benchmark;

  uint32_t numEntriesinTable = 0;
  int waysForCache = 8;
  bool disablePF = false;
  // stat
  uint64_t meta_table_lookups = 0;
  uint64_t meta_table_hits = 0;
  uint64_t meta_table_issued_prefetches = 0;
  uint64_t meta_table_accurate_prefetches = 0;
  std::set<uint64_t> meta_table_prefetches;

  resize_prMetaTable* metaTable = new resize_prMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  LRUSetAssociativeCache<std::deque<uint64_t>>* pcTable = new LRUSetAssociativeCache<std::deque<uint64_t>>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  uint64_t num_useful_prefetch = 0;
  uint64_t num_issued_prefetch = 0;
  uint64_t num_demand = 0;
  float resize_score, llc_hit_rate, useful_prefetch_rate;
  bool metadata_resized = false;

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
    //llc_cache->set_available_ways(16 - WAY_MARKOV);
    benchmark = champsim::global_trace_name;
    std::string trace_path(benchmark);
    std::string file_name = "/mnt/data/lyq/exprlog/hint/" + toProfilePath(trace_path) + ".txt";
    std::ifstream pc_file(file_name);
    if (!pc_file) {
      std::cerr << "Unable to open: " << file_name << endl;
      assert(false);
    }

    std::string line;
    std::getline(pc_file, line);
    int num_entries = std::stoi(line);

    if (num_entries == 0) {
      waysForCache = 16;
      disablePF = true;
    } else if (num_entries <= 4096 * 12 * 1) {
      waysForCache = 15;
    } else if (num_entries <= 4096 * 12 * 2) {
      waysForCache = 14;
    } else if (num_entries <= 4096 * 12 * 4) {
      waysForCache = 12;
    } else if (num_entries <= 4096 * 12 * 8) {
      waysForCache = 8;
    } else {
      assert(false && "Error: Incorrectly formatted line.");
    }

    if (!disablePF) {
      metaTable = new resize_prMetaTable(4096 * 12 * (16 - waysForCache), 12);
      metaTable->setpp(this);
    }
    llc_cache->set_available_ways(waysForCache);
    cout << "Alloc " << waysForCache << " ways for cache" << endl;

  
    cout << std::dec;
    pc_file.close();
  }

  bool isAlreadyInQueue(std::vector<uint64_t>& addresses, uint64_t addr)
  {
    for (uint64_t& a : addresses) {
      if (a == addr)
        return true;
    }
    return false;
  }

  int issue_metatable(resize_prMetaTable* metaTable, uint64_t pc, uint64_t lookup, std::vector<uint64_t>& addresses);

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

  void reset_metadata_size()
  {
    //deprecated
  };

  using champsim::modules::prefetcher::prefetcher;

  void prefetcher_initialize()
  {
    metaTable->setpp(this);
#ifdef MISS_CLASS_LOG
    benchmark = champsim::global_trace_name;
    log_file_name = "./" + toProfilePath(benchmark) + ".txt";
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

#endif // __MEM_CACHE_PREFETCH_resize_HH__
