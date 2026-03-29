#pragma once // 防止重复包含

#include <iostream>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>

#include "address.h"
#include "cache.h"
#include "champsim.h"
#include "modules.h"

#define MAX_DEGREE 4

#define CACHE_INDEX_BITS 12
#define CACHE_SETMASK ((1 << 12) - 1)
#define CACHE_ASSOC 16

#define TRNGL_TU_INDEX_BITS 5
#define TRNGL_TU_SETMASK ((1 << 5) - 1)
#define TRNGL_TU_ASSOC 16

#define TRNGL_HS_INDEX_BITS 8
#define TRNGL_HS_SETMASK ((1 << 8) - 1)
#define TRNGL_HS_ASSOC 2

#define TRNGL_SC_INDEX_BITS 5
#define TRNGL_SC_SETMASK ((1 << 5) - 1)
#define TRNGL_SC_ASSOC 2

#define TRNGL_RB_INDEX_BITS 7
#define TRNGL_RB_SETMASK ((1 << 7) - 1)
#define TRNGL_RB_ASSOC 2

#define TRNGL_MD_INDEX_BITS 12
#define TRNGL_MD_SETMASK ((1 << 12) - 1)
#define TRNGL_MD_ASSOC 96

#define TRNGL_SD_INDEX_BITS 6
#define TRNGL_SD_SETMASK ((1 << 6) - 1)
#define TRNGL_SD_CACHE_SIZE 16
#define TRNGL_SD_MARKOV_SIZE 8

#define DEBUG_PRINT 1
using namespace std;

enum ReplacementPolicy { LRU, FIFO };

class SatCounter
{
public:
  uint64_t value;
  uint64_t max;
  SatCounter(uint64_t bits, uint64_t init_value = 0)
  {
    value = init_value;
    max = (1 << bits) - 1;
  }

  SatCounter():value(0),max(0){

  }

  void set(uint64_t val) {
    value = val;
  }

  void increment(uint64_t by = 1)
  {
    if (max == 0)
      assert(false && "SatCounter not initialized");
    if (value + by <= max)
      value += by;
    
  }

  void decrement(uint64_t by = 1)
  {
    if (max == 0)
      assert(false && "SatCounter not initialized");
    if (value >= by)
      value -= by;
  }
  bool isMax() {
    if (max == 0)
      assert(false && "SatCounter not initialized");
    return value == max;
  }
  bool isMin() {
    if (max == 0)
      assert(false && "SatCounter not initialized");
    return value == 0;
  }
  bool operator>(const uint64_t& other) { return value > other; }
  bool operator<(const uint64_t& other) { return value < other; }
  bool operator>=(const uint64_t& other) { return value >= other; }
  bool operator<=(const uint64_t& other) { return value <= other; }
  bool operator==(const uint64_t& other) { return value == other; }
  bool operator!=(const uint64_t& other) { return value != other; }
  void operator=(const uint64_t& other) { value = other; }
};

template <class T>
class Cacheway
{
public:
  struct DLinkedNode {
    uint64_t key;
    T value;
    DLinkedNode* pre;
    DLinkedNode* post;
  };

  int count;
  int capacity;

  ReplacementPolicy policy;

  std::string owner;
  unordered_map<uint64_t, DLinkedNode*> cache;

  DLinkedNode* head;
  DLinkedNode* tail;

public:
  Cacheway(int capacity, ReplacementPolicy policy = LRU, const std::string& owner = "unknown") : count(0), capacity(capacity), policy(policy), owner(owner)
  {

    head = new DLinkedNode();
    head->pre = nullptr;

    tail = new DLinkedNode();
    tail->post = nullptr;

    head->post = tail;
    tail->pre = head;
  }

  Cacheway(const Cacheway& other) = delete;

  Cacheway(Cacheway&& other) = delete;

  Cacheway& operator=(Cacheway&& other) = delete;

  Cacheway& operator=(const Cacheway& other) = delete;

  ~Cacheway()
  {
    clear();
    delete head;
    delete tail;
  }

  // 清理函数
  void clear()
  {
    if (head != nullptr) {
      DLinkedNode* current = head->post;
      while (current != tail) {
        DLinkedNode* next = current->post;
        delete current;
        current = next;
      }
      head->post = tail;
      tail->pre = head;
      count = 0;
      cache.clear();
    }
  }

  int decrementCapacity(int n)
  {
    if (n >= capacity) {
      int total_evicted = count;
      clear();
      capacity = 0;
      return total_evicted;
    } else {
      if (count > capacity - n) {
        int to_evict = count - (capacity - n);
        int total_evicted = 0;
        for (int i = 0; i < to_evict; ++i) {
          DLinkedNode* tailNode = popTail();
          if (tailNode) {
            cache.erase(tailNode->key);
            --count;
            ++total_evicted;
            delete tailNode;
          }
        }
        capacity -= n;
        return total_evicted;
      } else {
        capacity -= n;
        return 0;
      }
    }
  }

  void incrementCapacity(int n)
  {
    if (n == 0)
      return;
    capacity += n;
  }

  T* find(uint64_t key)
  {
    if (cache.find(key) == cache.end()) {
      return nullptr;
    }
    DLinkedNode* node = cache[key];
    return &(node->value);
  }

  T* get(uint64_t key)
  {
    if (cache.find(key) == cache.end()) {
      return nullptr;
    }
    DLinkedNode* node = cache[key];
    if (policy == LRU)
      moveToHead(node);

    return &(node->value);
  }

  /* Return: Evicted or not. Best practice: first use get_victim(), then set() */
  bool set(uint64_t key, T value)
  {
    if (cache.find(key) == cache.end()) {
      DLinkedNode* newNode = new DLinkedNode();
      newNode->key = key;
      newNode->value = value;

      cache[key] = newNode;
      addNode(newNode);

      ++count;

      if (count > capacity) {
        DLinkedNode* tailNode = popTail();
        if (tailNode) {
          cache.erase(tailNode->key);
          --count;
          delete tailNode;
          return true;
        }
      }
    } else {
      DLinkedNode* node = cache[key];
      node->value = value;
      if (policy == LRU)
        moveToHead(node);
    }
    return false;
  }

  T* get_victim(uint64_t key)
  {
    if (cache.find(key) == cache.end()) {
      if (count + 1 > capacity) {
        DLinkedNode* tailNode = tail->pre;
        return tailNode == head ? nullptr : &(tailNode->value);
      } else {
        return nullptr;
      }
    } else {
      return &(cache[key]->value);
    }
  }

  void touch(uint64_t key)
  {
    if (policy == FIFO) {
      return;
    } else if (policy == LRU) {
      if (cache.find(key) == cache.end()) {
        return;
      }
      DLinkedNode* node = cache[key];
      moveToHead(node);
    }
  }

  DLinkedNode* begin() { return head->post; }

  DLinkedNode* end() { return tail; }

public:
  void addNode(DLinkedNode* node)
  {
    node->pre = head;
    node->post = head->post;

    head->post->pre = node;
    head->post = node;
  }

  void addNodeToTail(DLinkedNode* node)
  {
    node->post = tail;
    node->pre = tail->pre;
    tail->pre->post = node;
    tail->pre = node;
  }

  void removeNode(DLinkedNode* node)
  {
    assert(node != nullptr);
    assert(node != head && node != tail);

    DLinkedNode* pre = node->pre;
    DLinkedNode* post = node->post;

    pre->post = post;
    post->pre = pre;
    node->pre = nullptr;
    node->post = nullptr;
  }

  void moveToHead(DLinkedNode* node)
  {
    removeNode(node);
    addNode(node);
  }

  DLinkedNode* popTail()
  {
    if (tail->pre == head)
      return nullptr;
    DLinkedNode* res = tail->pre;
    removeNode(res);
    return res;
  }
};

template <class T>
class RRIPCacheway{
  // data, RRPV
  std::unordered_map<uint64_t, std::pair<T, SatCounter>> cache; // key -> T
  std::string owner;
  int capacity;
  int count;
  void aging() {
    for (auto& [k, v] : cache) {
      v.second.increment();
    }
  }
public:
  RRIPCacheway(int capacity, const std::string& owner = "unknown"):capacity(capacity), owner(owner), count(0) {}

  ~RRIPCacheway() {}

  int decrementCapacity(int n){
    if(n <= 0) return 0;
    if(capacity - n >= count) { // no eviction
      capacity -= n;
      return 0;
    } else {
      int to_evict = count - (capacity - n);
      int total_evicted = 0;
      for (int i = 0; i < to_evict; ++i) {
        uint64_t victim = 0;
        bool find = false;
        while (true) {
          for (auto& [k, v] : cache) {
            if (v.second.isMax()) {
              victim = k;
              find = true;
            } 
          }
          if(find) break;
          aging();
        }
        cache.erase(victim);
        count--;
        total_evicted++;
      }
      capacity -= n;
      return total_evicted;
    }
  }

  void incrementCapacity(int n){
    if (n == 0)
      return;
    capacity += n;
  }

  T* get(uint64_t key) {
    if (cache.find(key) == cache.end()) {
      return nullptr;
    }
    cache[key].second.set(0);
    return &(cache[key].first);
  }

  T* find(uint64_t key) {
    if(cache.find(key) == cache.end()) {
      return nullptr;
    }
    return &(cache[key].first);
  }

  void touch(uint64_t key) {
    if (cache.find(key) == cache.end()) {
      return;
    }
    cache[key].second.set(0);
  };

  uint64_t get_victim_key(uint64_t key) {
    if (find(key)) {
      return key;
    } else if (count + 1 > capacity) {
      while (true) {
        for (auto& [k, v] : cache) {
          if (v.second.isMax()) {
            return k;
          }
        }
        aging();
      }
    } else {
      return 0;
    }
  }

  bool set(uint64_t key, T value) {
    uint64_t victim_key = get_victim_key(key);// updates rrpv
    if(victim_key) {
      if(victim_key == key) {
        cache[victim_key].second.set(0); // reset
        cache[victim_key].first = value;
      } else {
        cache.erase(victim_key);
        cache.emplace(key,std::make_pair(value, SatCounter(3, 6)));
      }
      return true;
    } else {
      cache.emplace(key,std::make_pair(value, SatCounter(3, 6)));
      count++;
    }
    return false;
  };

  T* get_victim(uint64_t key) {
    if(T* t = find(key)) {
      return t;
    } else if(count+1 > capacity) {
      while (true) {
        for (auto& [k, v] : cache) {
          if (v.second.isMax()) {
            return &(v.first);
          }
        }
        aging();
      }
    } else {
      return nullptr;
    }
  };
};

template <typename T>
class AssociativeCache
{
public:
  std::vector<Cacheway<T>*> table;
  uint64_t count;
  uint64_t set_mask;
  uint64_t ways;

public:
  AssociativeCache(uint64_t sets, int ways, ReplacementPolicy policy, const std::string& owner = "unknown") : count(0), set_mask(sets - 1), ways(ways)
  {
    table.reserve(sets);
    for (size_t i = 0; i < sets; i++) {
      table.push_back(new Cacheway<T>(ways, policy, owner));
    }
    std::cout << owner << " initialized, " << ways << " ways * " << sets << " sets" << std::endl;
  }

  ~AssociativeCache()
  {
    for (auto& lru_cache : table) {
      delete lru_cache;
    }
  }

  /*
    without touching LRU
  */
  T* find(uint64_t key)
  {
    uint64_t set_index = key & set_mask;
    return table[set_index]->find(key);
  }

  T* get(uint64_t key)
  {
    uint64_t set_index = key & set_mask;
    return table[set_index]->get(key);
  }

  /**
    returns evicted or not
  */
  bool set(uint64_t key, T value)
  {
    uint64_t set_index = key & set_mask;
    bool evicted = table[set_index]->set(key, value);
    if (!evicted) {
      count++;
    }
    return evicted;
  }

  T* get_victim(uint64_t key)
  {
    uint64_t set_index = key & set_mask;
    return table[set_index]->get_victim(key);
  }

  void touch(uint64_t key)
  {
    uint64_t set_index = key & set_mask;
    table[set_index]->touch(key);
  }
};


struct TrainingUnitEntry {
  uint64_t key;
  uint64_t last_addr0;
  uint64_t last_addr1;
  uint64_t timestamp;
  uint64_t local_timestamp; // 添加：每个训练条目的本地时间戳
  SatCounter reuse_conf;
  SatCounter pattern_conf0;
  SatCounter pattern_conf1;
  SatCounter sample_rate;

  bool currently_twodist_pf; // 添加：控制是否使用两步预取

  TrainingUnitEntry()
      : key(0), last_addr0(0), last_addr1(0), timestamp(0), local_timestamp(0), reuse_conf(4, 7), pattern_conf0(4, 7), pattern_conf1(4, 7), sample_rate(4, 8),
        currently_twodist_pf(false)
  {
  }

  static uint64_t extractTag(uint64_t addr) { return addr >> TRNGL_TU_INDEX_BITS; }

  TrainingUnitEntry(uint64_t PC, uint64_t last_addr, uint64_t time)
      : key(PC), last_addr0(last_addr), last_addr1(0), timestamp(time), local_timestamp(0), reuse_conf(4, 7), pattern_conf0(4, 7), pattern_conf1(4, 7),
        sample_rate(4, 8), currently_twodist_pf(false)
  {
  }
};

struct MetadataEntry {
  uint64_t key;
  uint64_t target_addr;
  bool conf;

  bool used;

  MetadataEntry()
  {
    key = 0;
    target_addr = 0;
    conf = false;
    used = false;
  }

  static uint64_t extractTag(uint64_t addr)
  {
    const int CHUNK = 10;
    const uint64_t MASK = (1ULL << CHUNK) - 1;
    uint64_t tag = 0;
    // Process a fixed number of chunks to cover 64 bits;
    // higher bits beyond the address width are treated as 0 (padding).
    const int NUM_CHUNKS = (64 + CHUNK - 1) / CHUNK; // 7 chunks for 64-bit addr
    for (int i = 0; i < NUM_CHUNKS; ++i) {
      tag ^= (addr & MASK);
      addr >>= CHUNK;
    }
    return tag;
  }

  MetadataEntry(uint64_t addr, uint64_t target_addr) : key(addr), target_addr(target_addr), conf(false), used(false) {}
};

struct HistorySamplerEntry {
  uint64_t key;
  //TrainingUnitEntry* tu_entry;

  uint64_t tu_entry_key;

  uint64_t timestamp;   // local_timestamp
  uint64_t target_addr; // next

  bool reused;
  bool confident;

  HistorySamplerEntry()
  {
    key = 0;
    tu_entry_key = 0;
    target_addr = 0;
    timestamp = 0;
    reused = false;
    confident = false;
  }

  uint64_t extractTag(uint64_t addr) { return addr >> TRNGL_HS_INDEX_BITS; }

  HistorySamplerEntry(uint64_t addr, uint64_t tu_entry, uint64_t target_addr, uint64_t timestamp)
      : key(addr), tu_entry_key(tu_entry), target_addr(target_addr), timestamp(timestamp), reused(false), confident(false)
  {
  }

  bool operator==(const HistorySamplerEntry& other) const { return this->key == other.key; }

  bool operator!=(const HistorySamplerEntry& other) const { return this->key != other.key; }

  HistorySamplerEntry& operator=(const HistorySamplerEntry& other)
  {
    if (this != &other) { // Check for self-assignment
      this->key = other.key;
      this->tu_entry_key = other.tu_entry_key;
      this->target_addr = other.target_addr;
      this->timestamp = other.timestamp;
      this->reused = other.reused;
      this->confident = other.confident;
    }
    return *this;
  }
};

struct SecondChanceSamplerEntry {
  uint64_t key;
  uint64_t train_pc;
  uint64_t timestamp;
  bool used;

  SecondChanceSamplerEntry()
  {
    key = 0;
    train_pc = 0;
    timestamp = 0;
    used = false;
  }

  uint64_t extractTag(uint64_t addr) { return addr >> TRNGL_SC_INDEX_BITS; }

  SecondChanceSamplerEntry(uint64_t addr, uint64_t train_pc, uint64_t timestamp) : key(addr), train_pc(train_pc), timestamp(timestamp), used(false) {}

  SecondChanceSamplerEntry& operator=(const SecondChanceSamplerEntry& other)
  {
    if (this != &other) { // Check for self-assignment
      this->key = other.key;
      this->train_pc = other.train_pc;
      this->timestamp = other.timestamp;
      this->used = other.used;
    }
    return *this;
  }
};

struct SetDuellerEntry {
  uint64_t set; // which set this entry corresponds to (0 ~ 2047)

  Cacheway<uint64_t> cache_track{TRNGL_SD_CACHE_SIZE};
  Cacheway<uint64_t> markov_track{TRNGL_SD_MARKOV_SIZE};

  SetDuellerEntry() { set = 0; }

  SetDuellerEntry(uint64_t set) : set(set) {}
};

class SetDueller
{
public:
  std::unordered_map<uint64_t, SetDuellerEntry*> table;
  int sets;
  std::vector<uint64_t> dueller_counters; // Counters of allocating 0-8 ways for metadata
public:
  SetDueller(uint64_t sets = (1 << TRNGL_SD_INDEX_BITS)) : sets(sets), dueller_counters(9, 0) { Randomize(); }

  void Randomize()
  {
    for (auto& entry : table) {
      delete entry.second;
    }
    table.clear();
    std::set<uint64_t> used_indices;
    for (uint64_t i = 0; i < sets; i++) {
      uint64_t rnd = rand() & CACHE_SETMASK;
      while (used_indices.find(rnd) != used_indices.end()) {
        rnd = rand() & CACHE_SETMASK;
      }
      used_indices.insert(rnd);
      table[rnd] = new SetDuellerEntry(rnd);
    }
  }

  void TryCacheHit(uint64_t addr)
  {
    if (table.find(addr & CACHE_SETMASK) != table.end()) {
      // Get LRU cache track
      auto* node = table[addr & CACHE_SETMASK]->cache_track.find(addr);
      auto fucker = &table[addr & CACHE_SETMASK];
      if (node) {

        auto cur = table[addr & CACHE_SETMASK]->cache_track.begin();
        auto end = table[addr & CACHE_SETMASK]->cache_track.end();
        int position = 0;
        while (cur != end && cur->key != addr) {
          cur = cur->post;
          position++;
        }
        // maintain at least "position" ways for regular cache, so that current access can hit
        // ,i.e., allocate at most (16 - position) ways for Markov cache
        for (int j = 0; (j <= 16 - position) && (j <= 8); j++) {
          dueller_counters[j]++;
        }
        table[addr & CACHE_SETMASK]->cache_track.touch(addr);
      } else {
        table[addr & CACHE_SETMASK]->cache_track.set(addr, addr);
      }
    }
  }

  void TryMarkovHit(uint64_t addr)
  {
    if (table.find(addr & CACHE_SETMASK) != table.end()) {
      // Get LRU markov track
      auto* node = table[addr & CACHE_SETMASK]->markov_track.find(addr);
      if (node) {
        auto* cur = table[addr & CACHE_SETMASK]->markov_track.begin();
        auto* end = table[addr & CACHE_SETMASK]->markov_track.end();
        int position = 0;
        while (cur != end && cur->key != addr) {
          cur = cur->post;
          position++;
        }
        // allocate at least "position" ways for Markov cache, so that current access can hit
        for (int j = position; j <= 8; j++) {
          dueller_counters[j] += 12 / 2;
        }
        table[addr & CACHE_SETMASK]->markov_track.touch(addr);
      } else {
        table[addr & CACHE_SETMASK]->markov_track.set(addr, addr);
      }
    }
  }

  void ResetCounters()
  {
    for (auto& counter : dueller_counters) {
      counter = 0;
    }
    Randomize();
  }
  /* 决斗 */
  int Duel()
  {
    // Find the partition with the highest counter
    int max_index = 0;
    for (int i = 1; i <= 8; i++) {
      if (dueller_counters[i] > dueller_counters[max_index]) {
        max_index = i;
      }
    }
    // Here, max_index is the chosen partition
    // You can use this index to adjust your cache partitioning strategy
    // For example, you might want to allocate more ways to the chosen partition

    // After dueling, reset counters for the next round
    return max_index;
  }
};

class Metadata
{
public:
  std::vector<RRIPCacheway<MetadataEntry>*> table;
  uint64_t cache_way_allocated;
  uint64_t count;

public:
  Metadata(uint64_t set = (1 << TRNGL_MD_INDEX_BITS), int ways = TRNGL_MD_ASSOC) : cache_way_allocated(4), count(0)
  {
    table.reserve(set);
    for (size_t i = 0; i < set; i++) {
      table.push_back(new RRIPCacheway<MetadataEntry>(ways, "MD"));
    }
  }

  

  ~Metadata()
  {
    for (auto& lru_cache : table) {
      delete lru_cache;
    }
  }

  /*
    without touching LRU
  */
  MetadataEntry* find(uint64_t key)
  {
    uint64_t set_index = key & TRNGL_MD_SETMASK;
    return table[set_index]->find(MetadataEntry::extractTag(key));
  }

  MetadataEntry* get(uint64_t key)
  {
    uint64_t set_index = key & TRNGL_MD_SETMASK;
    return table[set_index]->get(MetadataEntry::extractTag(key));
  }

  /**
    returns evicted or not
  */
  bool set(uint64_t key, MetadataEntry value)
  {
    uint64_t set_index = key & TRNGL_MD_SETMASK;
    bool evicted = table[set_index]->set(MetadataEntry::extractTag(key), value);
    if (!evicted) {
      count++;
    }
    return evicted;
  }

  MetadataEntry* get_victim(uint64_t key)
  {
    uint64_t set_index = key & TRNGL_MD_SETMASK;
    return table[set_index]->get_victim(MetadataEntry::extractTag(key));
  }

  void touch(uint64_t key)
  {
    uint64_t set_index = key & TRNGL_MD_SETMASK;
    table[set_index]->touch(MetadataEntry::extractTag(key));
  }

  void repartition(uint64_t new_cache_way)
  {
    // If nothing changes, do nothing
    if (cache_way_allocated == new_cache_way)
      return;
    int total_evicted = 0;
    if (new_cache_way > cache_way_allocated) {
      // Increase way allocation
      for (auto& lru_cache : table) {
        lru_cache->incrementCapacity((new_cache_way - cache_way_allocated) * 12);
      }
    } else {
      // Decrease way allocation
      for (auto& lru_cache : table) {
        total_evicted += lru_cache->decrementCapacity((cache_way_allocated - new_cache_way) * 12);
      }
      count -= total_evicted;
    }
    cache_way_allocated = new_cache_way;
  }
};

template <typename T>
void removeDuplicates(std::vector<T>& vec)
{
  std::sort(vec.begin(), vec.end());
  auto last = std::unique(vec.begin(), vec.end());
  vec.erase(last, vec.end()); 
}

class triangel : public champsim::modules::prefetcher
{
public: // All members shall be basic variables, pointers, or implement proper copy/move constructors and assignment operators
  CACHE* llc_cache = nullptr;
  bool last_access_from_mrb = false;
  uint64_t global_timestamp = 0;
  uint64_t second_chance_timestamp = 0;
  int current_partition = 4; // Start with a neutral partition

  uint64_t MT_lookups = 0;
  uint64_t MT_hits = 0;
  uint64_t MT_lookup_reqs = 0;
  uint64_t MT_lookup_returns = 0;
  uint64_t MT_inserts = 0;

  AssociativeCache<TrainingUnitEntry>* TU = new AssociativeCache<TrainingUnitEntry>(1 << TRNGL_TU_INDEX_BITS, TRNGL_TU_ASSOC, LRU, "TU");
  AssociativeCache<HistorySamplerEntry>* HS = new AssociativeCache<HistorySamplerEntry>(1 << TRNGL_HS_INDEX_BITS, TRNGL_HS_ASSOC, LRU, "HS");
  AssociativeCache<SecondChanceSamplerEntry>* SC = new AssociativeCache<SecondChanceSamplerEntry>(1 << TRNGL_SC_INDEX_BITS, TRNGL_SC_ASSOC, FIFO, "SC");
  AssociativeCache<MetadataEntry>* RB = new AssociativeCache<MetadataEntry>(1 << TRNGL_RB_INDEX_BITS, TRNGL_RB_ASSOC, FIFO, "RB");
  Metadata* MD = new Metadata();
  SetDueller* SD = new SetDueller();

  SatCounter global_reuse_conf = SatCounter(7, 64);
  SatCounter global_pattern_conf0 = SatCounter(7, 64);
  SatCounter global_pattern_conf1 = SatCounter(7, 64);

  std::unordered_map<uint64_t, bool> cache_pf_map;

  bool RandomChance(int reuseConf, int sampleRate)
  {
    uint64_t threshold;
    if (sampleRate > 8) {
      threshold = ((1000000000l / 512) << (sampleRate - 8));
    } else {
      threshold = ((1000000000l / 512) >> (8 - sampleRate));
    }
    if (reuseConf < 3)
      threshold >>= 4; // No idea why.
    uint64_t random = rand() % 1000000000ul;
    return random < threshold;
  }

  void set_llc_reference(CACHE* llc)
  {
    llc_cache = llc;
    // initialize partition
    llc_cache->set_available_ways(16 - current_partition);
    MD->repartition(current_partition);
  }

  MetadataEntry* GetMetadata(uint64_t addr, bool use_rb)
  {
    if (use_rb) {
      auto RB_entry = RB->find(addr);
      if (RB_entry) {
        last_access_from_mrb = true;
        return RB_entry;
      }
    }

    auto MD_entry = MD->find(addr);
    if (MD_entry) {
      last_access_from_mrb = false;
      if (use_rb) {
        MetadataEntry RB_new = MetadataEntry(addr, MD_entry->target_addr);
        RB_new.conf = MD_entry->conf;
        RB->set(addr, RB_new);
      }
      return MD_entry;
    }
    return nullptr;
  }

  /*
    Add new metadata entry (addr -> target_addr) to Markov Partition
    Returns: Evict something or not
    Note: Checks whether the entry exists before calling this function
  */
  bool AddMetadata(uint64_t addr, uint64_t target_addr)
  {
    assert(addr != target_addr);

    auto MD_entry = MD->find(addr);
    if (MD_entry) {
      if (MD_entry->target_addr == target_addr) {
        MD_entry->conf = true;
      } else if (!MD_entry->conf) {
        MD_entry->target_addr = target_addr;
      } else {
        MD_entry->conf = false;
      }
      MD->touch(MD_entry->key);
      return false;
    } else {
      MetadataEntry MD_new = MetadataEntry(addr, target_addr);
      return MD->set(addr, MD_new);
    }
  }

  using champsim::modules::prefetcher::prefetcher;

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_cycle_operate();
  void prefetcher_final_stats();
};