#pragma once

#include "address.h"
#include "cache.h"
#include "champsim.h"
#include "modules.h"
#include "prism_framework.h"
#include <bits/stdc++.h>

using namespace std;

#define DETECT_UNIT_SIZE 32
#define TRAINING_UNIT_SIZE 16
#define TRAINING_UNIT_WINDOW 256
#define META_WAY_MAX 8
#define META_WAY_MIN 1
#define META_WAY_INIT 4

#define TRACKING_WINDOW 262144 // 2^18 accesses

template <typename T>
void removeDuplicates(std::vector<T>& vec)
{
  std::sort(vec.begin(), vec.end());
  auto last = std::unique(vec.begin(), vec.end());
  vec.erase(last, vec.end());
}

uint64_t hash_xor(uint64_t key, uint64_t width);

template <typename T>
class PseudoLRUCache {
public:
  struct CacheLine {
    uint64_t tag = 0;
    T data{};
    bool valid = false;
  };

  size_t ways;
  std::vector<CacheLine> cache;
  std::vector<uint8_t> tree; // 0: left MRU, 1: right MRU

  explicit PseudoLRUCache(size_t size)
      : ways(size), cache(size), tree(size - 1, 0) {
    assert((size & (size - 1)) == 0);
  }

  size_t getReplacementIndex() {
    for (size_t i = 0; i < ways; i++) {
      if (!cache[i].valid)
        return i;
    }

    size_t node = 0;
    while (node < tree.size()) {
      bool mru_right = tree[node];
      node = mru_right ? (2 * node + 1) : (2 * node + 2);
    }

    return node - (tree.size());
  }

  void updateTree(size_t way) {
    size_t node = way + tree.size();
    while (node > 0) {
      size_t parent = (node - 1) / 2;
      bool is_right = (node == 2 * parent + 2);
      tree[parent] = is_right;
      node = parent;
    }
  }

  CacheLine* find(uint64_t tag) {
    for (size_t i = 0; i < ways; i++) {
      if (cache[i].valid && cache[i].tag == tag) {
        updateTree(i);
        return &cache[i];
      }
    }
    return nullptr;
  }

  void insert(uint64_t tag, const T& data) {
    size_t idx = getReplacementIndex();
    cache[idx] = CacheLine{tag, data, true};
    updateTree(idx);
  }
};


struct DetectUnitEntry {
  DetectUnitEntry(uint64_t _ip_tag = 0, bool _key_ip = false, uint64_t _miss = 0) : ip_tag(_ip_tag), key_ip(_key_ip), miss_count(_miss) {}
  uint64_t ip_tag;
  bool key_ip;
  uint64_t miss_count;
};

// detect key ip that causes cache miss frequently
class DetectUnit
{
  PseudoLRUCache<DetectUnitEntry> table;
  uint64_t total_miss;

public:
  DetectUnit() : total_miss(0), table(DETECT_UNIT_SIZE) {}

  ~DetectUnit() { cout << "free DetectUnit" << endl; }

  bool update(uint64_t ip_tag)
  {
    if (total_miss > 511) {
      for (auto& i : table.cache) {
        i.data.miss_count = 0;
      }
      total_miss = 0;
    }

    auto entry = table.find(ip_tag);
    if (!entry) {
      table.insert(ip_tag, DetectUnitEntry(ip_tag, false, 1));
      total_miss++;
      return false;
    } else {
      entry->data.miss_count++;
      if (entry->data.key_ip)
        return true;

      total_miss++;
      if (entry->data.miss_count * 8 > total_miss) {
        entry->data.key_ip = true;
        return true;
      }
    }
    return false;
  }
};

enum Key_Tag_Type {
  NO_FOUND = -2,
  Negative = -1,
  Neutral = 0,
  Positive = 1,
};

struct TrainUnitEntry {
  TrainUnitEntry(uint64_t _last_addr = 0, uint64_t _useful_pf = 0, uint64_t _miss = 0, Key_Tag_Type _key_tag = Neutral)
      : last_addr(_last_addr), useful_pf_count(_useful_pf), miss_count(_miss), key_tag(_key_tag)
  {
  }
  uint64_t last_addr;
  uint64_t useful_pf_count;
  uint64_t miss_count;
  Key_Tag_Type key_tag;
};

class TrainUnit
{
  PseudoLRUCache<TrainUnitEntry> table;

public:
  TrainUnit() : table(TRAINING_UNIT_SIZE) {}

  ~TrainUnit() { cout << "free TrainUnit" << endl; }
  Key_Tag_Type update(uint64_t ip_tag, uint64_t& last_addr, uint64_t cur_addr, bool key_ip, bool cache_hit, bool useful_prefetch)
  {
    auto entry = table.find(ip_tag);
    if (!entry) {
      if (key_ip) {
        table.insert(ip_tag, TrainUnitEntry(cur_addr, useful_prefetch ? 1 : 0, 1, Neutral));
      }
      return NO_FOUND;
    } else {
      last_addr = entry->data.last_addr;
      entry->data.last_addr = cur_addr;
      if (!cache_hit || useful_prefetch) {
        entry->data.miss_count++;
      }
      if (useful_prefetch) {
        assert(cache_hit);
        entry->data.useful_pf_count++;
      }
      if (entry->data.miss_count >= TRAINING_UNIT_WINDOW) {
        double coverage = 1.0 * entry->data.useful_pf_count / entry->data.miss_count;
        if (coverage > 0.875) {
          entry->data.key_tag = Positive;
        } else if (coverage < 0.125) {
          entry->data.key_tag = Negative;
        }
        entry->data.miss_count = 0;
        entry->data.useful_pf_count = 0;
      }
      return entry->data.key_tag;
    }
  }
};

// Template for a set-associative LFU cache
// T: Type of data stored in the cache
template <typename T>
class LFUCache
{
public:
  struct CacheLine {
    uint64_t tag;         // Tag for identifying cache lines
    T data;               // Data stored in the cache line
    size_t frequency = 0; // Frequency counter for LFU
    bool high_priority = 0;
    bool valid = false;
  };

  std::vector<CacheLine> entry;

  LFUCache(size_t _size) : entry(_size) {}

  T* find(uint64_t key)
  {
    for (auto& i : entry) {
      if (i.valid && i.tag == buildTag(key)) {
        ++i.frequency;  // Increment frequency on access
        return &i.data; // Cache hit
      }
    }
    return nullptr; // Cache miss
  }

  bool insert(uint64_t key, const T& item, bool high_priority)
  {
    uint64_t tag = buildTag(key);
    for (auto& i : entry) {
      if (i.valid && i.tag == tag) {
        // Update existing entry
        i.data = item;
        i.high_priority = high_priority;
        return false; // No eviction
      }
    }

    // Find an invalid line or the least frequently used line within actual_way
    auto lfuIt = entry.begin();
    for (auto it = entry.begin(); it != entry.end(); ++it) {
      if (!it->valid) {
        lfuIt = it;
        break;
      }
      if (it->frequency < lfuIt->frequency) {
        lfuIt = it;
      }
    }

    bool evicted = lfuIt->valid;
    // Replace the cache line
    *lfuIt = {tag, item, 1, high_priority, true};
    return evicted; // Return whether an eviction occurred
  }

private:
  uint64_t buildTag(uint64_t key) { return key; }
};

struct MetaEntry {
  MetaEntry(uint64_t _last_addr = 0, uint64_t _target_addr = 0) : last_addr(_last_addr), target_addr(_target_addr) {}
  uint64_t last_addr;
  uint64_t target_addr;
};

class MetaDataTable
{
public:
  size_t actual_set;
  size_t actual_way;
  vector<LFUCache<MetaEntry>> table;

  // _actual_set : set number of the LLC
  ~MetaDataTable() { cout << "free MetaDataTable" << endl; }
  void initialize(size_t _actual_set)
  {
    actual_set = _actual_set;
    actual_way = META_WAY_INIT;
    table = std::vector<LFUCache<MetaEntry>>(actual_set, LFUCache<MetaEntry>(12 * actual_way));
  }

  MetaEntry* find(uint64_t last_addr)
  {
    size_t setIndex = getSetIndex(last_addr);
    auto& set = table[setIndex];

    // size_t wayIndex = getWayIndex(last_addr);
    // auto& line = set[wayIndex];

    return set.find(last_addr);
  }

  bool insert(uint64_t last_addr, uint64_t target_addr, bool high_priority)
  {
    size_t setIndex = getSetIndex(last_addr);
    auto& set = table[setIndex];

    // size_t wayIndex = getWayIndex(last_addr);
    // auto& line = set[wayIndex];

    return set.insert(last_addr, MetaEntry(last_addr, target_addr), high_priority);
  }

  void resize(int8_t direction)
  {
    if (direction > 0 && actual_way < META_WAY_MAX) {
      actual_way++;
      for (auto& set : table) {
        for (size_t i = 0; i < 12; i++) {
          set.entry.push_back(LFUCache<MetaEntry>::CacheLine());
        }
      }
    } else if (direction < 0 && actual_way > META_WAY_MIN) {
      actual_way--;
      for (auto& set : table) {
        for (size_t i = 0; i < 12; i++) {
          set.entry.pop_back();
        }
      }
    }
  }

private:
  size_t getSetIndex(uint64_t last_addr) { return last_addr % actual_set; }
};

class kairos : public champsim::modules::prefetcher
{
public:
  DetectUnit detect_unit;
  TrainUnit train_unit;
  MetaDataTable metadata;

  // stat
  uint64_t metadata_hit_count = 0;
  uint64_t current_window = 0;
  uint32_t increase_metadata = 0;
  uint32_t decrease_metadata = 0;
  uint32_t maintain_metadata = 0;

  // PID controller parameters
  const double ALPHA = 0.6;         // Prefetch utility weight
  const double BETA = -0.3;         // Miss sensitivity weight
  const double GAMMA = 0.1;         // Miss change rate weight
  const double THETA_PLUS = 0.5;    // Positive adjustment threshold
  const double THETA_MINUS = -0.25; // Negative adjustment threshold
  const double TAU = 1.2;           // Miss increase threshold

  // PID tracking
  uint64_t access_count = 0;
  uint64_t miss_count = 0;
  uint64_t prefetch_hit_count = 0;
  double previous_utility = 0.0;
  uint64_t previous_miss_count = 0;
  double previous_miss_rate = 0.0;
  double previous_delta_miss = 0.0;
  const int MAX_NO_IMPROVEMENT_WINDOWS = 2;
  const double IMPROVEMENT_THRESHOLD = 0.05; // 5% improvement threshold
  int no_improvement_count = 0;

  CACHE* llc_cache = nullptr;

  ~kairos()
  {
    cout << "free Kairos" << endl;
    llc_cache = nullptr;
  }
  void set_llc_reference(CACHE* llc)
  {
    llc_cache = llc;
    uint32_t waysForCache = 8;
    llc->set_available_ways(waysForCache);
    metadata.initialize(llc->NUM_SET);
  }

  void predict(uint64_t addr, int degree, vector<uint64_t>& prefetch_candidate)
  {
    while (degree > 0) {
      auto entry = metadata.find(addr);
      if (entry) {
        metadata_hit_count++;
        if (entry->target_addr == 0 || entry->target_addr == addr)
          break;
        prefetch_candidate.push_back(entry->target_addr);
        addr = entry->target_addr;
      } else {
        break;
      }
      degree--;
    }
  }

  // Function to evaluate tracking window and make sizing decisions
  void evaluate_window()
  {
    // Calculate metrics for this window
    double miss_rate = 1.0 * miss_count / TRACKING_WINDOW;
    double utility = 0.0;

    // Calculate prefetch utility (η)
    if (miss_count > 0) {
      utility = 1.0 * prefetch_hit_count / miss_count;
    }

    // Calculate miss rate sensitivity (ΔM)
    double delta_miss = 0.0;
    if (current_window > 0) {
      delta_miss = 1.0 * (miss_count - previous_miss_count) / access_count;
    }

    // Calculate adjustment using PID-like formula
    double delta_k = ALPHA * utility + BETA * delta_miss + GAMMA * (delta_miss - previous_delta_miss);

    // Resize decision making
    int8_t resize_decision = 0;
    if (metadata.actual_way < META_WAY_MAX) {
      if (delta_k > THETA_PLUS || (1.0 * prefetch_hit_count / access_count) > (1.0 * metadata.actual_way / META_WAY_MAX))
        resize_decision = 1; // Increase
    } else if (metadata.actual_way > META_WAY_MIN) {
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

    metadata.resize(resize_decision);
    if (resize_decision > 0) {
      increase_metadata++;
    } else if (resize_decision < 0) {
      decrease_metadata++;
    } else {
      maintain_metadata++;
    }

    if (llc_cache != nullptr) {
      uint32_t available_ways = llc_cache->NUM_WAY - metadata.actual_way;
      llc_cache->set_available_ways(available_ways);

      if constexpr (champsim::debug_print) {
        std::cout << "[kairos] LLC partition updated: metadata_ways=" << metadata.actual_way << ", available_ways=" << available_ways << std::endl;
      }
    }

    previous_utility = utility;
    previous_miss_count = miss_count;
    previous_delta_miss = delta_miss;
    previous_miss_rate = miss_rate;
    miss_count = 0;
    prefetch_hit_count = 0;
    current_window++;
  };

  using champsim::modules::prefetcher::prefetcher;

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};