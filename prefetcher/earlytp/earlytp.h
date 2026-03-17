#ifndef EARLYTP
#define EARLYTP

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

#define PCQ_SIZE 8
#define ONLY_TRIGGER_ON_MISS false
#define PC_META_TABLE_MODE 0 // 0: map ; 1: lru ; 2: srrip
#define PC_META_TABLE_SIZE (4 * 1024 * 1 * 12)
#define PC_META_TABLE_ASSOC 12

#define PC_TABLE_SIZE 512
#define PC_TABLE_ASSOC 16

#define META_TABLE_SIZE 393216
#define META_TABLE_ASSOC 12
#define GLOBAL_DEGREE 1 

class earlytp;

uint64_t hash_xor(uint64_t addr);

struct earlytpMetaTableEntry {

  uint64_t correlated_addr;
  bool used;

  earlytpMetaTableEntry() : correlated_addr(0), used(false) {};
  earlytpMetaTableEntry(uint64_t addr) : correlated_addr(addr) {};
};

class earlytpMetaTable : public LRUSetAssociativeCache<earlytpMetaTableEntry>
{
  typedef LRUSetAssociativeCache<earlytpMetaTableEntry> Super;

public:
  std::unordered_map<uint64_t, std::set<uint64_t>> reverse_metatable;
  earlytp* prefetcher;

  earlytpMetaTable(int size, int num_ways) : Super(size, num_ways), prefetcher(nullptr) {}

  void setpp(earlytp* p) { prefetcher = p; }

  earlytpMetaTableEntry* find(uint64_t key)
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
  bool insert(uint64_t key, const earlytpMetaTableEntry& data);

  Entry* erase(uint64_t key) { return Super::erase(key); }
};

class earlytp : public champsim::modules::prefetcher
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

  earlytpMetaTable* metaTable = new earlytpMetaTable(META_TABLE_SIZE, META_TABLE_ASSOC);

  uint64_t global_timestamp = 0;
  struct PCTableEntry
  {
    uint64_t timestamp;
    std::deque<uint64_t> addrHistory;
  };

  LRUSetAssociativeCache<PCTableEntry>* pcTable = new LRUSetAssociativeCache<PCTableEntry>(PC_TABLE_SIZE, PC_TABLE_ASSOC);

  std::deque<uint64_t> PCQ;
#if PC_META_TABLE_MODE == 0
  std::map<uint64_t, uint64_t> pc_meta_table; // <trigger pc, block_addr>
#elif PC_META_TABLE_MODE == 1
  LRUSetAssociativeCache<uint64_t>* pc_meta_table = new LRUSetAssociativeCache<uint64_t>(PC_META_TABLE_SIZE, PC_META_TABLE_ASSOC);
#elif PC_META_TABLE_MODE == 2
  SRRIPSetAssociativeCache<uint64_t>* pc_meta_table = new SRRIPSetAssociativeCache<uint64_t>(PC_META_TABLE_SIZE, PC_META_TABLE_ASSOC);
#endif
  // stat
  std::set<uint64_t> PCM_issued_prefetches, PCM_filled_prefetches;
  uint64_t PCM_late_prefetches = 0, PCM_useful_prefetches = 0, PCM_useless_prefetches = 0;

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
    llc_cache->set_available_ways(8);
  }

  bool isAlreadyInQueue(std::vector<uint64_t>& addresses, uint64_t addr)
  {
    for (uint64_t& a : addresses) {
      if (a == addr)
        return true;
    }
    return false;
  }

  int issue_metatable(earlytpMetaTable* metaTable, uint64_t lookup, std::vector<uint64_t>& addresses);

  void invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t>& pref_addr);

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
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};

#endif // __MEM_CACHE_PREFETCH_earlytp_HH__
