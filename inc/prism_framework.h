#ifndef PRISM_FRAMEWORK
#define PRISM_FRAMEWORK

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define MAX_RRPV 3

using namespace std;

template <class T>
class SetAssociativeCache
{
public:
  class Entry
  {
  public:
    uint64_t key;
    uint64_t index;
    uint64_t tag;
    bool valid;
    T data;
  };

  SetAssociativeCache(int size, int num_ways, int debug_level = 0)
      : size(size), num_ways(num_ways), num_sets(size / num_ways), entries(num_sets, vector<Entry>(num_ways)), cams(num_sets), debug_level(debug_level)
  {
    // assert(size % num_ways == 0);
    for (int i = 0; i < num_sets; i += 1)
      for (int j = 0; j < num_ways; j += 1)
        entries[i][j].valid = false;
    /* calculate `index_len` (number of bits required to store the index) */
    for (int max_index = num_sets - 1; max_index > 0; max_index >>= 1)
      this->index_len += 1;
  }

  /**
   * Invalidates the entry corresponding to the given key.
   * @return A pointer to the invalidated entry
   */
  Entry* erase(uint64_t key)
  {
    Entry* entry = this->find(key);
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    auto& cam = cams[index];
    // int num_erased = cam.erase(tag);
    cam.erase(tag);
    if (entry)
      entry->valid = false;
    // assert(entry ? num_erased == 1 : num_erased == 0);
    return entry;
  }

  /**
   * @return The old state of the entry that was updated
   */
  Entry insert(uint64_t key, const T& data)
  {
    Entry* entry = this->find(key);
    if (entry != nullptr) {
      Entry old_entry = *entry;
      entry->data = data;
      return old_entry;
    }
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    vector<Entry>& set = this->entries[index];
    int victim_way = -1;
    for (int i = 0; i < this->num_ways; i += 1)
      if (!set[i].valid) {
        victim_way = i;
        break;
      }
    if (victim_way == -1) {
      victim_way = this->select_victim(index);
    }
    Entry& victim = set[victim_way];
    Entry old_entry = victim;
    victim = {key, index, tag, true, data};
    auto& cam = cams[index];
    if (old_entry.valid) {
      // int num_erased = cam.erase(old_entry.tag);
      cam.erase(old_entry.tag);
      // assert(num_erased == 1);
    }
    cam[tag] = victim_way;
    return old_entry;
  }

  Entry* find(uint64_t key)
  {
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    auto& cam = cams[index];
    if (cam.find(tag) == cam.end())
      return nullptr;
    int way = cam[tag];
    Entry& entry = this->entries[index][way];
    // assert(entry.tag == tag && entry.valid);
    return &entry;
  }

  int find_way(uint64_t key)
  {
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    auto& cam = cams[index];
    if (cam.find(tag) == cam.end())
      return -1;
    int way = cam[tag];
    return way;
  }

  int get_index_len() { return this->index_len; }

  void set_debug_level(int debug_level) { this->debug_level = debug_level; }

  /**
   * @return The set index
   */
  virtual uint64_t get_index(uint64_t key) { return key % this->num_sets; }

  /**
   * @return The tag
   */
  virtual uint64_t get_tag(uint64_t key) { return key / this->num_sets; }

  virtual int select_victim(uint64_t index)
  {
    /* random eviction policy if not overriden */
    return rand() % this->num_ways;
  }

  vector<Entry> get_valid_entries()
  {
    vector<Entry> valid_entries;
    for (int i = 0; i < num_sets; i += 1)
      for (int j = 0; j < num_ways; j += 1)
        if (entries[i][j].valid)
          valid_entries.push_back(entries[i][j]);
    return valid_entries;
  }

  int size;
  int num_ways;
  int num_sets;
  int index_len = 0; /* in bits */
  vector<vector<Entry>> entries;
  vector<unordered_map<uint64_t, int>> cams;
  int debug_level = 0;
};

template <class T>
class FIFOSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  FIFOSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level), fifo_ptr(this->num_sets, 0) {}

protected:
  /* @override */
  virtual int select_victim(uint64_t index) override
  {
    int victim = fifo_ptr[index];
    fifo_ptr[index] = (fifo_ptr[index] + 1) % this->num_ways;
    return victim;
  }
  std::vector<uint32_t> fifo_ptr;
};

template <class T>
class LFUSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  LFUSetAssociativeCache(int size, int num_ways, int debug_level = 0)
      : Super(size, num_ways, debug_level), freq(this->num_sets, std::vector<uint64_t>(num_ways, 0)), global_ts(0)
  {
  }

  void touch(uint64_t key)
  {
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    freq[index][way]++;
  }

  void set_default(uint64_t key)
  {
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    freq[index][way] = 0;
  }

  std::vector<std::vector<uint64_t>> freq;
  uint64_t global_ts;

protected:
  /* @override */
  virtual int select_victim(uint64_t index)
  {
    int victim_way = 0;
    uint64_t min_freq = -1;
    for (int way = 0; way < this->num_ways; ++way) {
      if (!this->entries[index][way].valid) {
        victim_way = way;
        return victim_way;
      }
      int curr_freq = freq[index][way];
      if (curr_freq < min_freq) {
        min_freq = curr_freq;
        victim_way = way;
      }
    }
    return victim_way;
  }
};

template <class T>
class RandomSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  RandomSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level), rng(std::random_device{}()) {}

protected:
  /* @override */
  virtual int select_victim(uint64_t index)
  {
    static std::uniform_int_distribution<int> dist;
    return dist(rng, decltype(dist)::param_type(0, this->num_ways - 1));
  }
  std::mt19937 rng;
};

template <class T>
class LRUSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  LRUSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level), lru(this->num_sets, vector<uint64_t>(num_ways)) {}

  void set_mru(uint64_t key) { *this->get_lru(key) = this->t++; }

  void set_lru(uint64_t key) { *this->get_lru(key) = 0; }

  vector<vector<uint64_t>> lru;
  uint64_t t = 1;

protected:
  /* @override */
  virtual int select_victim(uint64_t index)
  {
    vector<uint64_t>& lru_set = this->lru[index];
    return min_element(lru_set.begin(), lru_set.end()) - lru_set.begin();
  }

  uint64_t* get_lru(uint64_t key)
  {
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    // assert(this->cams[index].count(tag) == 1);
    int way = this->cams[index][tag];
    return &this->lru[index][way];
  }
};

template <class T>
class SHIPSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  SHIPSetAssociativeCache(int size, int num_ways, int debug_level = 0)
      : Super(size, num_ways, debug_level), rrpv(this->num_sets, vector<uint64_t>(num_ways)), sampler((1 << LOG2_SAMPLER_SET), vector<SamplerEntry>(12)),
        shct(SHCT_SIZE)
  {
  }
  static const int SHCT_SIZE = 16384;
  static const int SHCT_PRIME = 16381;
  static const int LOG2_SAMPLER_SET = 8;

  // static const int SHCT_SIZE = 4096; // 4096*3/8=1536
  // static const int SHCT_PRIME = 4093;
  // static const int LOG2_SAMPLER_SET = 6;

  static const int SHCT_MAX = 7;

  class SamplerEntry
  {
  public:
    bool valid = false;
    bool used = false;
    uint64_t addr = 0;
    uint64_t ip = 0;
    uint64_t last_used = 0;
  };
  /*
    0 - Sampled_set
    else - follower
  */
  int which_set(uint64_t key)
  {
    int rem = key & ((1 << (15 - LOG2_SAMPLER_SET)) - 1);
    return rem;
  }

  /* @override */
  int select_victim(uint64_t index) override
  {
    vector<uint64_t>& rrpv_set = this->rrpv[index];
    while (true) {
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] == MAX_RRPV) {
          return i;
        }
      }
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] < MAX_RRPV) {
          rrpv_set[i]++;
        }
      }
    }
  }

  void touch(uint64_t key, uint64_t ip)
  {
    update_repl(key, ip);
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    this->rrpv[index][way] = 0;
  }

  void set_default(uint64_t key, uint64_t ip)
  { // for insertion
    update_repl(key, ip);
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    this->rrpv[index][way] = MAX_RRPV - 1;
    if (shct[(ip & 0xFFFFFFFF) % SHCT_PRIME] == SHCT_MAX) {
      this->rrpv[index][way] = MAX_RRPV;
    }
  }

  void set_rrpv(uint64_t key, uint64_t rrpv_value)
  { // for insertion
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    this->rrpv[index][way] = rrpv_value;
  }

  // 0 16 32 48 -> 0 1 2 3
  void update_repl(uint64_t key, uint64_t ip)
  {
    if (which_set(key) == 0) { // is sampled
      auto s_idx = (key >> (15 - LOG2_SAMPLER_SET)) & ((1 << LOG2_SAMPLER_SET) - 1);
      SamplerEntry* se = nullptr;
      for (auto& s : sampler[s_idx]) {
        if (s.valid && s.addr == key) {
          se = &s;
          break;
        }
      }
      if (se) {
        auto SHCT_idx = (se->ip & 0xFFFFFFFF) % SHCT_PRIME;
        if (shct[SHCT_idx] > 0)
          shct[SHCT_idx]--;
        se->used = true;
        se->last_used = access_count++;
      } else {
        SamplerEntry* se_min = nullptr;
        uint64_t min = -1;
        for (auto& s : sampler[s_idx]) {
          if (s.last_used < min) {
            se_min = &s;

            min = s.last_used;
          }
        }
        if (!se_min->used) {
          auto SHCT_idx = (se_min->ip & 0xFFFFFFFF) % SHCT_PRIME;
          if (shct[SHCT_idx] < SHCT_MAX)
            shct[SHCT_idx]++;
        }
        se_min->valid = true;
        se_min->addr = key;
        se_min->ip = ip;
        se_min->used = false;
        se_min->last_used = access_count++;
      }
    }
  }

  vector<vector<uint64_t>> rrpv;
  vector<vector<SamplerEntry>> sampler; // 256 sampled * 12 way
  vector<uint64_t> shct;
  uint64_t access_count = 0;
};

template <class T>
class SRRIPSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  SRRIPSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level), rrpv(this->num_sets, vector<uint64_t>(num_ways))
  {
  }

  /* @override */
  int select_victim(uint64_t index) override
  {
    vector<uint64_t>& rrpv_set = this->rrpv[index];
    while (true) {
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] == MAX_RRPV) {
          return i;
        }
      }
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] < MAX_RRPV) {
          rrpv_set[i]++;
        }
      }
    }
  }

  void touch(uint64_t key) { this->decrement(key); }

  void set_default(uint64_t key)
  { // for insertion
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    this->rrpv[index][way] = MAX_RRPV - 1;
  }

  void set_rrpv(uint64_t key, uint64_t rrpv_value)
  { // for insertion
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    this->rrpv[index][way] = rrpv_value;
  }

  void decrement(uint64_t key)
  { // for touch
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
#ifdef TOUCH_DECREMENT
    if (this->rrpv[index][way] > 0)
      this->rrpv[index][way]--;
#else
    this->rrpv[index][way] = 0;
#endif
  }

  vector<vector<uint64_t>> rrpv;
};

template <class T>
class BRRIPSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  BRRIPSetAssociativeCache(int size, int num_ways, int debug_level = 0)
      : Super(size, num_ways, debug_level), rrpv(this->num_sets, vector<uint64_t>(num_ways)), bip_counter(0)
  {
  }

  /* @override */
  int select_victim(uint64_t index) override
  {
    vector<uint64_t>& rrpv_set = this->rrpv[index];
    while (true) {
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] == MAX_RRPV) {
          return i;
        }
      }
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] < MAX_RRPV) {
          rrpv_set[i]++;
        }
      }
    }
  }

  void touch(uint64_t key) { this->decrement(key); }

  void set_default(uint64_t key)
  { // for insertion
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    bip_counter++;
    if (bip_counter >= 32) {
      this->rrpv[index][way] = MAX_RRPV - 1;
      bip_counter = 0;
    } else {
      this->rrpv[index][way] = MAX_RRPV;
    }
  }

  void set_rrpv(uint64_t key, uint64_t rrpv_value)
  { // for insertion
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    this->rrpv[index][way] = rrpv_value;
  }

  void decrement(uint64_t key)
  { // for touch
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
#ifdef TOUCH_DECREMENT
    if (this->rrpv[index][way] > 0)
      this->rrpv[index][way]--;
#else
    this->rrpv[index][way] = 0;
#endif
  }

  uint64_t bip_counter;
  vector<vector<uint64_t>> rrpv;
};

template <class T>
class DRRIPSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  DRRIPSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level), rrpv(this->num_sets, vector<uint64_t>(num_ways))
  {
  }
  static const int BIP_CHANCE = 32;
  static const int MAX_PSEL = 1023;
  const int mask = (1 << 6) - 1;

  /* @override */
  int select_victim(uint64_t index) override
  {
    vector<uint64_t>& rrpv_set = this->rrpv[index];
    while (true) {
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] == MAX_RRPV) {
          return i;
        }
      }
      for (size_t i = 0; i < Super::num_ways; i++) {
        if (rrpv_set[i] < MAX_RRPV) {
          rrpv_set[i]++;
        }
      }
    }
  }

  /*
    0 - SRRIP leader
    1 - BIP leader
    else - follower
  */
  int which_set(uint64_t key)
  { // totally 32768 sets, select 2 leaders from each 512 sets
    uint64_t index = this->get_index(key);
    if ((index & mask) == 0)
      return 1; // srrip leader
    else if ((index & mask) == 1)
      return -1; // bip leader
    else
      return 0; // follower
  }

  void touch(uint64_t key) { this->decrement(key); }

  void set_default(uint64_t key)
  { // for insertion
    int set_type = which_set(key);
    if (set_type > 0) { // srrip leader
      update_replacement(key, true);
      if (psel > 0)
        psel--;
    } else if (set_type < 0) { // bip leader
      update_replacement(key, false);
      if (psel < MAX_PSEL)
        psel++;
    } else { // follower
      update_replacement(key, psel <= MAX_PSEL / 2);
    }
  }

  void update_replacement(uint64_t key, bool isSRRIP)
  {
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
    if (isSRRIP) {
      this->rrpv[index][way] = MAX_RRPV - 1;
    } else {
      this->rrpv[index][way] = MAX_RRPV;
      bip_counter++;
      if (bip_counter == BIP_CHANCE) {
        bip_counter = 0;
        this->rrpv[index][way] = MAX_RRPV - 1;
      }
    }
  }

  void decrement(uint64_t key)
  { // for touch
    uint64_t index = this->get_index(key);
    uint64_t tag = this->get_tag(key);
    int way = this->cams[index][tag];
#ifdef TOUCH_DECREMENT
    if (this->rrpv[index][way] > 0)
      this->rrpv[index][way]--;
#else
    this->rrpv[index][way] = 0;
#endif
  }
  uint64_t bip_counter;
  uint64_t psel;
  vector<vector<uint64_t>> rrpv;
};

uint64_t hash_index(uint64_t key, int index_len);
template <class T>
inline T square(T x)
{
  return x * x;
}

#endif /* PRISM_FRAMEWORK */
