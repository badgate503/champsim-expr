#ifndef PROPHET
#define PROPHET

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

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16

#define META_TABLE_SIZE (96 * N_LLC_SET)

#define META_TABLE_ASSOC 12
#define MRB_TABLE_SIZE 65536
#define MRB_TABLE_ASSOC 16
#define MRB_MAX_COUNTER 3
#define GLOBAL_DEGREE 4 // metatable & mrb_table has this degree

// #define IS_TRAIN
#define ENABLE_MRB true

class prophet;

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

struct ProphetMetaTableEntry {

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
  prophet* pp;

  ProphetMetaTable(int size, int num_ways) : Super(size, num_ways), priority_pgo(num_sets, vector<uint8_t>(num_ways, 0)) {}

  void setpp(prophet* p) { pp = p; }

  ProphetMetaTableEntry* find(uint64_t key)
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
  bool insert(uint64_t key, const ProphetMetaTableEntry& data, uint8_t priority = 0);

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
        if (min_lru > lru_set[i]) {
          min_lru = lru_set[i];
          victim_index = i;
        }
      }
    }
    return victim_index;
  }

  vector<vector<uint8_t>> priority_pgo;
};

struct ProphetMRBTableEntry {
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

  ProphetMRBTableEntry* find(uint64_t key)
  {
    Entry* entry = Super::find(key);
    if (!entry) {
      return nullptr;
    }
    return &(entry->data);
  }

  void insert(uint64_t key, const ProphetMRBTableEntry& data)
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
        if (min_lru > lru_set[i]) {
          min_lru = lru_set[i];
          victim_index = i;
        }
      }
    }
    return victim_index;
  }
};

class prophet : public champsim::modules::prefetcher
{
public:
  // BaseTags* cachetags;
  CACHE* llc_cache = NULL;
  int debug_level = 0;
#ifdef IS_TRAIN
  bool inTraining = true;
#else
  bool inTraining = false;
#endif
  bool enableMRB = ENABLE_MRB;
#ifdef IS_TRAIN
  int globalDegree = 1;
#else
  int globalDegree = GLOBAL_DEGREE;
#endif
  bool disablePF = false;
  std::string trace_path;
  bool enableInsertFilter = true;
  bool enablePGLRU = true;
  std::map<uint64_t, int> profileReplTable;
  std::map<uint64_t, int32_t> profileUtiTable;
  std::set<uint64_t> profileInsertTable;

  uint32_t numEntriesinTable = 0;
  long long global_timestamp = 0;
  uint32_t allMisses = 0;
  int waysForCache = 8;

  // stat MT lookups
  uint64_t MT_lookups = 0;
  uint64_t MT_hits = 0;
  uint64_t MT_inserts = 0;
  uint64_t MT_lookup_reqs = 0;
  uint64_t MT_lookup_returns = 0;

  uint64_t rb_read_n = 0;
  uint64_t rb_write_n = 0;
  uint64_t md_read_n = 0;
  uint64_t md_write_n = 0;
  uint64_t tu_read_n = 0;
  uint64_t tu_write_n = 0;

  bool warmup_reset = false;
  void reset_stat_counters()
  {
    MT_lookups = 0;
    MT_hits = 0;
    MT_inserts = 0;
    MT_lookup_reqs = 0;
    MT_lookup_returns = 0;

    rb_read_n = 0;
    rb_write_n = 0;
    md_read_n = 0;
    md_write_n = 0;
    tu_read_n = 0;
    tu_write_n = 0;

    warmup_reset = true;
  }

  std::map<uint64_t, TrainEntry> trainTable;

  ProphetMetaTable* metaTable;

  ProphetMRBTable* mrbTable = new ProphetMRBTable(MRB_TABLE_SIZE, MRB_TABLE_ASSOC);

  LRUSetAssociativeCache<uint64_t>* pcTable = new LRUSetAssociativeCache<uint64_t>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  std::set<uint64_t> metaUsedPool;

  std::set<uint64_t> metaInsertedPool;

  std::map<uint64_t, uint64_t> prefetched_addr; // <block_addr, trigger pc>

  std::string log_file_name;
  std::ofstream logfile;
  std::string hint_file_name;
  std::ofstream hint_file;
  bool warmup_complete = false;

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

  std::string getTraceDir(const std::string& full_path)
  {
    // this function is to extract trace dir from the input trace path
    size_t last_slash = full_path.find_last_of('/');
    if (last_slash == std::string::npos) {
      return "";
    }
    std::string dir_part = full_path.substr(0, last_slash);

    last_slash = dir_part.find_last_of('/');
    if (last_slash == std::string::npos) {
      return "";
    }
    dir_part = dir_part.substr(0, last_slash);

    return dir_part;
  }

  void set_llc_reference(CACHE* llc)
  {
    llc_cache = llc;

    trace_path = champsim::global_trace_array[champsim::cur_cpu];
    std::string trace_dir = getTraceDir(trace_path);
    std::cout << "cpu = " << champsim::cur_cpu << ", tracename = " << getTraceName(trace_path) << std::endl;
    champsim::cur_cpu++;
#ifdef ELABORATE_LOG
    log_file_name = "./" + getTraceName(trace_path) + ".txt";
    cout << log_file_name << endl;
    logfile.open(log_file_name);
#endif

    if (inTraining) {
      llc_cache = llc;
      llc_cache->set_available_ways(8);
      cout << "Alloc 8 ways for cache" << endl;

      metaTable = new ProphetMetaTable(META_TABLE_SIZE, 12);
      metaTable->setpp(this);

      hint_file_name = trace_dir + "/hint/" + getTraceName(trace_path) + ".txt";
      cout << "hint file: " << hint_file_name << endl;
      hint_file.open(hint_file_name);
      if (!hint_file) {
        std::cerr << "Unable to open: " << hint_file_name << endl;
        assert(false);
      }
    } else {
      std::string file_name = trace_dir + "/hint/" + getTraceName(trace_path) + ".txt";
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
      } else if (num_entries <= N_LLC_SET * 12 * 1) {
        waysForCache = 15;
      } else if (num_entries <= N_LLC_SET * 12 * 2) {
        waysForCache = 14;
      } else if (num_entries <= N_LLC_SET * 12 * 4) {
        waysForCache = 12;
      } else if (num_entries <= N_LLC_SET * 12 * 8) {
        waysForCache = 8;
      } else {
        waysForCache = 8;
      }

      if (!disablePF) {
        metaTable = new ProphetMetaTable(N_LLC_SET * 12 * (16 - waysForCache), 12);
        metaTable->setpp(this);
      }
      llc_cache->set_available_ways(waysForCache);
      cout << "Alloc " << waysForCache << " ways for cache" << endl;

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

  int issue_metatable(ProphetMetaTable* metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses);
  int issue_mrbtable(ProphetMRBTable* metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses);

  void invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t>& pref_addr);

  void outPrefetcherPGOInfo();

  uint64_t get_last(uint64_t ip)
  {
    auto pc_entry = pcTable->find(ip);
    if (pc_entry) {
      return pc_entry->data;
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
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};

#endif // __MEM_CACHE_PREFETCH_Prophet_HH__
