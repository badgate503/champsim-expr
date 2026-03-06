#ifndef LTP
#define LTP

#include <cassert>
#include <cstdint>
#include <deque>
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
#include "morton_filter/morton_sample_configs.h"

#define FILTER_MODE 0 // 0: no filter; 1: ideal; 2: directly map table
#define META_TABLE_SIZE 393216
#define META_TABLE_ASSOC 12
#define MRB_TABLE_SIZE 65526
#define MRB_TABLE_ASSOC 16
#define MRB_MAX_COUNTER 3
#define GLOBAL_DEGREE 1 // metatable & mrb_table has this degree

#define ENABLE_MRB false
#define ENABLE_PGO false

#define GPQ_SIZE 4
#define LATE_THRESHOLD 0.125
#define HIGH_ACCURACY_THRESHOLD 0.5
#define LOW_ACCURACY_THRESHOLD 0.125
#define PF_FILTER_SIZE 512

class ltp;

struct SingleMetaEntry {
  uint64_t correlatedAddr;
  SatCounter8 counter;

  SingleMetaEntry(unsigned bits) : correlatedAddr(0), counter(bits) {}
};

struct MetaEntry : public TaggedEntry {
  /** group of stides */
  std::vector<SingleMetaEntry> entries;
  bool used;
  uint64_t pc;
  long long touch_time;
  MetaEntry() : TaggedEntry(), entries(1, 1), used(false), pc(0), touch_time(0) {};
  MetaEntry(size_t num_strides, unsigned counter_bits) : TaggedEntry(), entries(num_strides, counter_bits), used(false), pc(0), touch_time(0) {}

  /** Reset the entries to their initial values */
  void invalidate() override
  {
    TaggedEntry::invalidate();
    used = false;
    pc = 0;
    touch_time = 0;
    for (auto& entry : entries) {
      entry.correlatedAddr = 0;
      entry.counter.reset();
    }
  }
};

struct TrainEntry {
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

struct ltpMetaTableEntry {

  uint64_t correlatedAddr;
  // int counter;
  bool used;
  // uint64_t pc;
  ltpMetaTableEntry() : correlatedAddr(0), used(false) {};
  ltpMetaTableEntry(uint64_t addr) : correlatedAddr(addr) {};
};

class ltpMetaTable : public LRUSetAssociativeCache<ltpMetaTableEntry>
{
  typedef LRUSetAssociativeCache<ltpMetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  ltp* pp;

  ltpMetaTable(int size, int num_ways) : Super(size, num_ways), priority_pgo(num_sets, vector<uint8_t>(num_ways, 0)) {}

  void setpp(ltp* p) { pp = p; }

  ltpMetaTableEntry* find(uint64_t key)
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
  bool insert(uint64_t key, const ltpMetaTableEntry& data, uint8_t priority = 0);
  Entry* erase(uint64_t key) { return Super::erase(key); }

  /* @override */
  int select_victim(uint64_t index)
  {
    uint8_t min_priority = 255;
    uint64_t min_lru = UINT64_MAX;
    int victim_index = 0;
    vector<uint8_t>& priority_set = this->priority_pgo[index];
    vector<uint64_t>& lru_set = this->lru[index];
    for (size_t i = 0; i < num_ways; i++) {
      if (min_priority > priority_set[i]) {
        min_priority = priority_set[i];
        min_lru = lru_set[i];
        victim_index = i;
      } else if (min_priority == priority_set[i]) {
        if (min_lru < lru_set[i]) {
          min_lru = lru_set[i];
          victim_index = i;
        }
      }
    }
    return victim_index;
  }

  vector<vector<uint8_t>> priority_pgo;
};

struct ltpMRBTableEntry {
  uint64_t correlatedAddr;
  uint8_t counter;
  ltpMRBTableEntry() : correlatedAddr(0), counter(0) {};
  ltpMRBTableEntry(uint64_t addr) : correlatedAddr(addr), counter(0) {};
};

class ltpMRBTable : public LRUSetAssociativeCache<ltpMRBTableEntry>
{
  typedef LRUSetAssociativeCache<ltpMRBTableEntry> Super;

public:
  ltpMRBTable(int size, int num_ways) : Super(size, num_ways)
  {
    // assert(__builtin_popcount(size) == 1);
  }

  ltpMRBTableEntry* find(uint64_t key)
  {
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    return &(entry->data);
  }

  void insert(uint64_t key, const ltpMRBTableEntry& data)
  {
    Super::insert(key, data);
    Super::set_mru(key);
  }

  Entry* erase(uint64_t key) { return Super::erase(key); }

  /* @override */
  int select_victim(uint64_t index)
  {
    uint8_t min_counter = UINT8_MAX;
    uint64_t min_lru = UINT64_MAX;
    vector<uint64_t>& lru_set = this->lru[index];
    int victim_index = 0;
    for (size_t i = 0; i < num_ways; i++) {
      if (!entries[index][i].valid) {
        return i;
      } else if (entries[index][i].data.counter < min_counter) {
        min_counter = entries[index][i].data.counter;
        victim_index = i;
      } else if (entries[index][i].data.counter == min_counter) {
        if (min_lru < lru_set[i]) {
          min_lru = lru_set[i];
          victim_index = i;
        }
      }
    }
    return victim_index;
  }
};

struct PCTableEntry {
  uint64_t lastAddr;
  uint64_t lastlastAddr;
  bool lookahead;
  uint64_t degree;
  uint32_t latePrefetchHistory; // bit counter
  uint32_t coverageHistory;
  uint64_t latePrefetchCount;
  uint64_t accuratePrefetchCount;
  uint64_t issuedPrefetchCount;
  uint64_t filledPrefetchCount;
  uint64_t missCount;

  PCTableEntry(uint64_t _lastAddr = 0, bool cache_hit = false)
      : lastAddr(_lastAddr), lastlastAddr(0), lookahead(true), degree(8), latePrefetchHistory(0), coverageHistory(0), latePrefetchCount(0),
        accuratePrefetchCount(0), issuedPrefetchCount(0), filledPrefetchCount(0), missCount(cache_hit ? 0 : 1) {};
  void shift_counter()
  {
    float accuracy = 1.0 * accuratePrefetchCount / filledPrefetchCount;

    if (degree < 3 && latePrefetchCount >= 4 && accuracy > 0.5) {
      degree += 1;
    } else if (degree < 6 && latePrefetchCount >= 8 && accuracy > 0.75) {
      degree += 1;
    }

    if (accuracy < LOW_ACCURACY_THRESHOLD) {
      lookahead = false;
      if (degree > 1) {
        degree -= 1;
      }
    }

    latePrefetchCount >>= 1;
    accuratePrefetchCount >>= 1;
    issuedPrefetchCount >>= 1;
    filledPrefetchCount >>= 1;
    missCount >>= 1;
  };
};

struct GlobalPCEntry {
  uint64_t ip;
  bool hit;
  uint64_t timestamp;
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

class ltp : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;
  bool enablePGO = ENABLE_PGO;
  bool enableMRB = ENABLE_MRB;
  int globalDegree = GLOBAL_DEGREE;
  bool disablePF = false;
  std::string benchmark;
  bool enableInsertFilter = true;
  bool enablePGLRU = true;
  std::map<uint64_t, int> profileReplTable;
  std::map<uint64_t, int32_t> profileUtiTable;
  std::set<uint64_t> profileInsertTable;

  uint32_t numEntriesinTable = 0;
  long long global_timestamp = 0;
  uint32_t allMisses = 0;
  int waysForCache = 8;
  /**
   * Information used to create a new PC table. All of them behave equally.
   */

  std::map<uint64_t, TrainEntry> trainTable;

  ltpMetaTable* metaTable;

  ltpMRBTable* mrbTable = new ltpMRBTable(MRB_TABLE_SIZE, MRB_TABLE_ASSOC);

  std::unordered_map<uint64_t, PCTableEntry> pcTable;

  std::set<uint64_t> metaUsedPool;

  std::set<uint64_t> metaInsertedPool;

  std::map<uint64_t, uint64_t> prefetched_addr; // <block_addr, trigger pc>

#if FILTER_MODE == 1
  std::set<uint64_t> no_need_prefetch;
#elif FILTER_MODE == 2
  PF_Filter pf_filter = PF_Filter(PF_FILTER_SIZE, 8);
#elif FILTER_MODE == 3
  CompressedCuckoo::Morton3_8* mf = new CompressedCuckoo::Morton3_8{8192};
#endif

#ifdef PC_TRIGGER_PREFETCH
  std::deque<GlobalPCEntry> GPQ;
  std::map<uint64_t, uint64_t> GPMetaTable; // <trigger pc, block_addr>
#endif

  std::string log_file_name;
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

    // std::string profile_path = "/mnt/data/lyq/Kairos/expr/hint/"+ base_name + ".txt";
    return base_name;
  }
  void set_llc_reference(CACHE* llc)
  {
    llc_cache = llc;
    benchmark = champsim::global_trace_name;

    log_file_name = "./" + toProfilePath(benchmark) + ".txt";
    cout << log_file_name << endl;
    logfile.open(log_file_name);

    if (!ENABLE_PGO) {
      metaTable = new ltpMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);
      metaTable->setpp(this);
    } else {
      std::string trace_path(benchmark);
      std::string file_name = "/mnt/data/lyq/exprlog/hint/" + toProfilePath(trace_path) + ".txt";
      // std::string file_name = "profile.txt";
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
      } else if (num_entries < 4096 * 12 * 1) {
        waysForCache = 15;
      } else if (num_entries < 4096 * 12 * 2) {
        waysForCache = 14;
      } else if (num_entries < 4096 * 12 * 4) {
        waysForCache = 12;
      } else if (num_entries < 4096 * 12 * 8) {
        waysForCache = 8;
      } else {
        assert(false && "Error: Incorrectly formatted line.");
      }

      if (!disablePF) {
        metaTable = new ltpMetaTable(META_TABLE_SIZE, (16 - waysForCache) * 12);
        metaTable->setpp(this);
      }
      llc_cache->set_available_ways(waysForCache);
      cout << "Alloc " << waysForCache << " for cache" << endl;

      while (std::getline(pc_file, line)) {
        std::istringstream lineStream(line);
        std::string PC;
        std::string priority;

        if (std::getline(lineStream, PC, ',') && std::getline(lineStream, priority, ',')) {
          // PC and degree are now split into two variables
          if (std::stoull(priority) == 0)
            continue;
          cout << "Add PC=0x" << hex << std::stoull(PC, nullptr, 16) << endl;
          profileInsertTable.insert(std::stoull(PC, nullptr, 16));
          profileReplTable[std::stoull(PC, nullptr, 16)] = std::stoull(priority);
        } else {
          std::cerr << "Error: Incorrectly formatted line." << endl;
        }
      }
      cout << std::dec;
      pc_file.close();
    }
  }
  void addToUsedPool(uint64_t a)
  {
    if (metaUsedPool.find(a) == metaUsedPool.end()) {
      metaUsedPool.insert(a);
    }
  }

  void addToInsertedPool(uint64_t a)
  {
    if (metaInsertedPool.find(a) == metaInsertedPool.end()) {
      metaInsertedPool.insert(a);
    }
  }

  bool isAlreadyInQueue(std::vector<uint64_t>& addresses, uint64_t addr)
  {
    for (uint64_t& a : addresses) {
      if (a == addr)
        return true;
    }
    return false;
  }

  int issue_metatable(ltpMetaTable* metaTable, uint64_t lookup, uint64_t pc, uint64_t degree, std::vector<uint64_t>& addresses);
  int issue_mrbtable(ltpMRBTable* metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses);

  void outPrefetcherPGOInfo();

  uint64_t get_last(uint64_t ip)
  {
    if (pcTable.find(ip) != pcTable.end()) {
      return pcTable[ip].lastAddr;
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
    /*
      --- init in function set_llc_reference! ---
      --- init in function set_llc_reference! ---
      --- init in function set_llc_reference! ---
    */
  }
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in, std::string latepf);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in, champsim::address ip);
  void prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};

#endif
