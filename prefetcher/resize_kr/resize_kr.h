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

#include "bakshalipour_framework.h"
#include "cache.h"
#include "champsim.h"

#define INIT_WINDOW 100000
#define BASE_TRIGGER_NUM 1
// #define MISS_CLASS_LOG

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16
#define WAY_MARKOV 8
#define META_TABLE_SIZE (4096 * 12 * 4)
#define META_TABLE_ASSOC 12
#define GLOBAL_DEGREE 1
#define TRACKING_WINDOW 262144
class resize_kr;

struct resize_krMetaTableEntry {

  uint64_t correlated_addr;
  bool used;

  resize_krMetaTableEntry() : correlated_addr(0), used(false) {};
  resize_krMetaTableEntry(uint64_t addr) : correlated_addr(addr) {};
};

class resize_krMetaTable : public SRRIPSetAssociativeCache<resize_krMetaTableEntry>
{
  typedef SRRIPSetAssociativeCache<resize_krMetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  resize_kr* prefetcher;

  resize_krMetaTable(int size, int num_ways) : Super(size, num_ways), prefetcher(nullptr) {}

  void setpp(resize_kr* p) { prefetcher = p; }

  resize_krMetaTableEntry* find(uint64_t key)
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
  bool insert(uint64_t key, const resize_krMetaTableEntry& data);
  bool insert(uint64_t key, const resize_krMetaTableEntry& data, uint64_t rrpv_value);

  Entry* erase(uint64_t key) { return Super::erase(key); }
};

class resize_kr : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;
  int globalDegree = GLOBAL_DEGREE;

  std::string benchmark;

  uint32_t numEntriesinTable = 0;
  int waysForCache = 8;

  // stat
  uint64_t meta_table_lookups = 0;
  uint64_t meta_table_hits = 0;
  uint64_t meta_table_issued_prefetches = 0;
  uint64_t meta_table_accurate_prefetches = 0;
  std::set<uint64_t> meta_table_prefetches;

  resize_krMetaTable* metaTable = new resize_krMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  LRUSetAssociativeCache<std::deque<uint64_t>>* pcTable = new LRUSetAssociativeCache<std::deque<uint64_t>>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  
  uint64_t num_issued_prefetch = 0;
  float resize_score, llc_hit_rate, useful_prefetch_rate;
  bool metadata_resized = false;

  std::string log_file_name;
  std::string hint_file;
  std::ofstream logfile;
  bool warmup_complete = false;

  /* Kairos resize policy */
  uint64_t metadata_hit_count = 0;
  uint64_t current_window = 0;
  uint32_t increase_metadata = 0;
  uint32_t decrease_metadata = 0;
  uint32_t maintain_metadata = 0;
  uint64_t num_useful_prefetch = 0;
  uint64_t num_misses = 0;
  uint64_t access_count = 0;
  // PID controller parameters
  const double ALPHA = 0.6;         // Prefetch utility weight
  const double BETA = -0.3;         // Miss sensitivity weight
  const double GAMMA = 0.1;         // Miss change rate weight
  const double THETA_PLUS = 0.5;    // Positive adjustment threshold
  const double THETA_MINUS = -0.25; // Negative adjustment threshold
  const double TAU = 1.2;           // Miss increase threshold
  // PID tracking
  double previous_utility = 0.0;
  uint64_t previous_num_misses = 0;
  double previous_miss_rate = 0.0;
  double previous_delta_miss = 0.0;
  const int MAX_NO_IMPROVEMENT_WINDOWS = 2;
  const double IMPROVEMENT_THRESHOLD = 0.05; // 5% improvement threshold
  int no_improvement_count = 0;
  int current_partition = 4;

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
    llc_cache->set_available_ways(16 - WAY_MARKOV);
  }

  bool isAlreadyInQueue(std::vector<uint64_t>& addresses, uint64_t addr)
  {
    for (uint64_t& a : addresses) {
      if (a == addr)
        return true;
    }
    return false;
  }

  int issue_metatable(resize_krMetaTable* metaTable, uint64_t pc, uint64_t lookup, std::vector<uint64_t>& addresses);

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

  void metadata_resize(int partition)
  {
    auto new_metaTable = new resize_krMetaTable(4096 * 12 * partition, 12);
    new_metaTable->setpp(this);
    for (int i = 0; i < metaTable->num_sets; i++) {
      for (int j = 0; j < metaTable->entries[i].size(); j++) {
        if (metaTable->entries[i][j].valid) // key % 8*4096 = 8; key % 2*4096 = 8 ; -> 2
          new_metaTable->insert(metaTable->entries[i][j].key, metaTable->entries[i][j].data, metaTable->rrpv[i][j]);
      }
    }
    cout << "Resize Metadata table. Alloc " << partition << " ways for metadata!" << endl;
    delete metaTable;
    metaTable = new_metaTable;
  }

  // Function to evaluate tracking window and make sizing decisions
  void evaluate_window()
  {
    // Calculate metrics for this window
    double miss_rate = 1.0 * num_misses / TRACKING_WINDOW;
    double utility = 0.0;

    // Calculate prefetch utility (η)
    if (num_misses > 0) {
      utility = 1.0 * num_useful_prefetch / num_misses;
    }

    // Calculate miss rate sensitivity (ΔM)
    double delta_miss = 0.0;
    if (current_window > 0) {
      delta_miss = 1.0 * (num_misses - previous_num_misses) / access_count;
    }

    // Calculate adjustment using PID-like formula
    double delta_k = ALPHA * utility + BETA * delta_miss + GAMMA * (delta_miss - previous_delta_miss);

    // Resize decision making
    int8_t resize_decision = 0;
    if (current_partition < 8) {
      if (delta_k > THETA_PLUS || (1.0 * num_useful_prefetch / access_count) > (1.0 * current_partition / 8))
        resize_decision = 1; // Increase
    } else if (current_partition > 1) {
      if (delta_k < THETA_MINUS && miss_rate > TAU * previous_miss_rate)
        resize_decision = -1; // Decrease
    }

    // No resize if no improvement over several windows
    if (current_window > 0) {
      double relative_improvement = previous_miss_rate - miss_rate;
      if (relative_improvement < IMPROVEMENT_THRESHOLD) {
        no_improvement_count++;
        if (no_improvement_count >= MAX_NO_IMPROVEMENT_WINDOWS) {
          resize_decision = 0;
        }
      } else {
        no_improvement_count = 0;
      }
    }

    
    current_partition += resize_decision;
    metadata_resize(current_partition);
    if (resize_decision > 0) {
      increase_metadata++;
    } else if (resize_decision < 0) {
      decrease_metadata++;
    } else {
      maintain_metadata++;
    }

    if (llc_cache != nullptr) {
      uint32_t available_ways = llc_cache->NUM_WAY - current_partition;
      llc_cache->set_available_ways(available_ways);

      if constexpr (champsim::debug_print) {
        std::cout << "[kairos] LLC partition updated: metadata_ways=" << current_partition << ", available_ways=" << available_ways << std::endl;
      }
    }

    previous_utility = utility;
    previous_num_misses = num_misses;
    previous_delta_miss = delta_miss;
    previous_miss_rate = miss_rate;
    num_misses = 0;
    num_useful_prefetch = 0;
    current_window++;
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
