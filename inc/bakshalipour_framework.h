#ifndef BAKSHALIPOUR_FRAMEWORK
#define BAKSHALIPOUR_FRAMEWORK

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

/**
 * A class for printing beautiful data tables.
 * It's useful for logging the information contained in tabular structures.
 */

class Table
{
public:
  Table(int width, int height) : width(width), height(height), cells(height, vector<string>(width)) {}

  void set_row(int row, const vector<string>& data, int start_col = 0)
  {
    // assert(data.size() + start_col == this->width);
    for (unsigned col = start_col; col < this->width; col += 1)
      this->set_cell(row, col, data[col]);
  }

  void set_col(int col, const vector<string>& data, int start_row = 0)
  {
    // assert(data.size() + start_row == this->height);
    for (unsigned row = start_row; row < this->height; row += 1)
      this->set_cell(row, col, data[row]);
  }

  void set_cell(int row, int col, string data)
  {
    // assert(0 <= row && row < (int)this->height);
    // assert(0 <= col && col < (int)this->width);
    this->cells[row][col] = data;
  }

  void set_cell(int row, int col, double data)
  {
    ostringstream oss;
    oss << setw(11) << fixed << setprecision(8) << data;
    this->set_cell(row, col, oss.str());
  }

  void set_cell(int row, int col, int64_t data)
  {
    ostringstream oss;
    oss << setw(11) << std::left << data;
    this->set_cell(row, col, oss.str());
  }

  void set_cell(int row, int col, int data) { this->set_cell(row, col, (int64_t)data); }

  void set_cell(int row, int col, uint64_t data)
  {
    ostringstream oss;
    oss << "0x" << setfill('0') << setw(16) << hex << data;
    this->set_cell(row, col, oss.str());
  }

  /**
   * @return The entire table as a string
   */
  string to_string()
  {
    vector<int> widths;
    for (unsigned i = 0; i < this->width; i += 1) {
      int max_width = 0;
      for (unsigned j = 0; j < this->height; j += 1)
        max_width = max(max_width, (int)this->cells[j][i].size());
      widths.push_back(max_width + 2);
    }
    string out;
    out += Table::top_line(widths);
    out += this->data_row(0, widths);
    for (unsigned i = 1; i < this->height; i += 1) {
      out += Table::mid_line(widths);
      out += this->data_row(i, widths);
    }
    out += Table::bot_line(widths);
    return out;
  }

  string data_row(int row, const vector<int>& widths)
  {
    string out;
    for (unsigned i = 0; i < this->width; i += 1) {
      string data = this->cells[row][i];
      data.resize(widths[i] - 2, ' ');
      out += " | " + data;
    }
    out += " |\n";
    return out;
  }

  static string top_line(const vector<int>& widths) { return Table::line(widths, "┌", "┬", "┐"); }

  static string mid_line(const vector<int>& widths) { return Table::line(widths, "├", "┼", "┤"); }

  static string bot_line(const vector<int>& widths) { return Table::line(widths, "└", "┴", "┘"); }

  static string line(const vector<int>& widths, string left, string mid, string right)
  {
    string out = " " + left;
    for (unsigned i = 0; i < widths.size(); i += 1) {
      int w = widths[i];
      for (int j = 0; j < w; j += 1)
        out += "─";
      if (i != widths.size() - 1)
        out += mid;
      else
        out += right;
    }
    return out + "\n";
  }

private:
  unsigned width;
  unsigned height;
  vector<vector<string>> cells;
};

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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    auto& cam = cams[index];
    if (cam.find(tag) == cam.end())
      return -1;
    int way = cam[tag];
    return way;
  }

  /**
   * Creates a table with the given headers and populates the rows by calling `write_data` on all
   * valid entries contained in the cache. This function makes it easy to visualize the contents
   * of a cache.
   * @return The constructed table as a string
   */
  string log(vector<string> headers)
  {
    vector<Entry> valid_entries = this->get_valid_entries();
    Table table(headers.size(), valid_entries.size() + 1);
    table.set_row(0, headers);
    for (unsigned i = 0; i < valid_entries.size(); i += 1)
      this->write_data(valid_entries[i], table, i + 1);
    return table.to_string();
  }

  int get_index_len() { return this->index_len; }

  void set_debug_level(int debug_level) { this->debug_level = debug_level; }

  /* should be overriden in children */
  virtual void write_data(Entry& entry, Table& table, int row) {}

  /**
   * @return The way of the selected victim
   */
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    freq[index][way]++;
  }

  void set_default(uint64_t key)
  {
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    this->rrpv[index][way] = 0;
  }

  void set_default(uint64_t key, uint64_t ip)
  { // for insertion
    update_repl(key, ip);
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    this->rrpv[index][way] = MAX_RRPV - 1;
    if (shct[(ip & 0xFFFFFFFF) % SHCT_PRIME] == SHCT_MAX) {
      this->rrpv[index][way] = MAX_RRPV;
    }
  }

  void set_rrpv(uint64_t key, uint64_t rrpv_value)
  { // for insertion
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    this->rrpv[index][way] = MAX_RRPV - 1;
  }

  void set_rrpv(uint64_t key, uint64_t rrpv_value)
  { // for insertion
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    this->rrpv[index][way] = rrpv_value;
  }

  void decrement(uint64_t key)
  { // for touch
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
  BRRIPSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level), rrpv(this->num_sets, vector<uint64_t>(num_ways)), bip_counter(0)
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    bip_counter++;
    if (bip_counter >= 32){
      this->rrpv[index][way] = MAX_RRPV - 1;
      bip_counter = 0;
    }else{
      this->rrpv[index][way] = MAX_RRPV;
    }
  }

  void set_rrpv(uint64_t key, uint64_t rrpv_value)
  { // for insertion
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    this->rrpv[index][way] = rrpv_value;
  }

  void decrement(uint64_t key)
  { // for touch
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
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

template <class T>
class TTSRRIPSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  TTSRRIPSetAssociativeCache(int size, int num_ways, int debug_level = 0)
      : Super(size, num_ways, debug_level), rrpv(this->num_sets, vector<uint64_t>(num_ways)), cams2(this->num_sets)
  {
  }

protected:
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

  typename SetAssociativeCache<T>::Entry tt_insert(uint64_t key, const T& data, uint64_t target)
  {
    typename SetAssociativeCache<T>::Entry old_entry = SetAssociativeCache<T>::insert(key, data);

    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    auto& cam = cams2[index];
    if (old_entry.valid) {
      // int num_erased = cam.erase(old_entry.tag);
      cam.erase(old_entry.tag);
      // assert(num_erased == 1);
    }
    cam[tag] = target;

    this->set_default(key, target);

    return old_entry;
  }

  void touch(uint64_t trigger, uint64_t target) { this->decrement(trigger, target); }

  void set_default(uint64_t trigger, uint64_t target)
  { // for insertion
    uint64_t index = trigger % this->num_sets;
    uint64_t tag = trigger / this->num_sets;

    int way = this->cams[index][tag];
    uint64_t actual_target = cams2[index][tag];
    if (actual_target == target)
      this->rrpv[index][way] = MAX_RRPV - 1;
  }

  void decrement(uint64_t trigger, uint64_t target)
  { // for touch
    uint64_t index = trigger % this->num_sets;
    uint64_t tag = trigger / this->num_sets;
    int way = this->cams[index][tag];
    uint64_t actual_target = cams2[index][tag];
    if (actual_target == target) {
      if (this->rrpv[index][way] > 0)
        this->rrpv[index][way]--;
      // std::cout<<"Touched " << std::hex<< trigger << "->" << target <<std::dec << std::endl;
    } else {
      // std::cout<<"Fail to touch " << std::hex<< trigger << "->" << target <<std::dec << std::endl;
    }
  }
  vector<unordered_map<uint64_t, uint64_t>> cams2; // index, tag -> target
  vector<vector<uint64_t>> rrpv;
};

template <class T>
class MJSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  MJSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level) {}

  // const int LLC_SET = 4096;
  // const int LLC_WAY = 96;
  // const int LOG2_LLC_SET = 12;
  const int LLC_SET = 4096 * 8;
  const int LLC_WAY = 12;
  const int LOG2_LLC_SET = 12 + 3;
  const int LOG2_BLOCK_SIZE = 6;

  const int LOG2_SAMPLED_SETS = 5; // 32 sampled sets

  const int HISTORY = 8;
  const int GRANULARITY = 8;

  const int INF_RD = LLC_WAY * HISTORY - 1;
  const int INF_ETR = (LLC_WAY * HISTORY / GRANULARITY) - 1;
  const int MAX_RD = INF_RD - 22;

  // each sampled set —— using 16-set, 5-way to track history
  const int SAMPLED_CACHE_WAYS = 5;
  const int LOG2_SAMPLED_CACHE_SETS = 4;
  const int SAMPLED_CACHE_TAG_BITS = 31 - LOG2_LLC_SET; // the same to markov-entry tag bit
  const int PC_SIGNATURE_BITS = 12;
  const int TIMESTAMP_BITS = 8;

  const double TEMP_DIFFERENCE = 1.0 / 16.0;
  const double FLEXMIN_PENALTY = 2.0 - 0 / 4.0;

  struct SampledCacheLine {
    bool valid;
    uint64_t tag;
    uint64_t signature;
    int timestamp;
  };

  bool is_sampled_set(int set)
  {
    int mask_length = LOG2_LLC_SET - LOG2_SAMPLED_SETS;
    int mask = (1 << mask_length) - 1;
    return (set & mask) == ((set >> (LOG2_LLC_SET - mask_length)) & mask);
  } // totally 2^LOG2_SAMPLED_SETS

  uint64_t CRC_HASH(uint64_t _blockAddress)
  {
    static const unsigned long long crcPolynomial = 3988292384ULL;
    unsigned long long _returnVal = _blockAddress;
    for (unsigned int i = 0; i < 3; i++)
      _returnVal = ((_returnVal & 1) == 1) ? ((_returnVal >> 1) ^ crcPolynomial) : (_returnVal >> 1);
    return _returnVal;
  }

  uint64_t get_pc_signature(uint64_t pc, bool hit)
  {

    pc = pc << 1;
    if (hit) {
      pc = pc | 1;
    }
    pc = CRC_HASH(pc);
    pc = (pc << (64 - PC_SIGNATURE_BITS)) >> (64 - PC_SIGNATURE_BITS);

    return pc;
  }

  uint32_t get_sampled_cache_index(uint64_t full_addr)
  {
    //  16 * 2048
    full_addr = (full_addr << (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET))) >> (64 - (LOG2_SAMPLED_CACHE_SETS + LOG2_LLC_SET));
    // remove higher bits
    return full_addr;
  }

  uint64_t get_sampled_cache_tag(uint64_t x)
  {
    x >>= LOG2_LLC_SET + LOG2_SAMPLED_CACHE_SETS;
    x = (x << (64 - SAMPLED_CACHE_TAG_BITS)) >> (64 - SAMPLED_CACHE_TAG_BITS); // only 10 bit tag
    return x;
  }

  int search_sampled_cache(uint64_t blockAddress, uint32_t set)
  {
    std::vector<SampledCacheLine>& sampled_set = sampled_cache.at(set);
    for (int way = 0; way < SAMPLED_CACHE_WAYS; way++) {
      if (sampled_set[way].valid && (sampled_set[way].tag == blockAddress)) {
        return way;
      }
    }
    return -1;
  }

  void detrain(uint32_t set, int way)
  {
    SampledCacheLine temp = sampled_cache[set][way];
    if (!temp.valid) {
      return;
    }

    if (rdp.count(temp.signature)) {
      rdp[temp.signature] = min(rdp[temp.signature] + 1, INF_RD);
    } else {
      rdp[temp.signature] = INF_RD;
    }
    sampled_cache[set][way].valid = false;
  }

  int temporal_difference(int init, int sample) // soft update
  {
    if (sample > init) {
      int diff = sample - init;
      diff = diff * TEMP_DIFFERENCE;
      diff = min(1, diff);
      return min(init + diff, INF_RD);
    } else if (sample < init) {
      int diff = init - sample;
      diff = diff * TEMP_DIFFERENCE;
      diff = min(1, diff);
      return max(init - diff, 0);
    } else {
      return init;
    }
  }

  int increment_timestamp(int input)
  {
    input++;
    input = input % (1 << TIMESTAMP_BITS);
    return input;
  }

  int time_elapsed(int global, int local)
  {
    if (global >= local) {
      return global - local;
    }
    global = global + (1 << TIMESTAMP_BITS);
    return global - local;
  }

protected:
  /* @override */
  int select_victim(uint64_t index) override
  {
    // your eviction policy goes here
    int max_etr = 0;
    int victim_way = 0;
    for (int way = 0; way < LLC_WAY; way++) {
      if (abs(etr[index][way]) > max_etr || (abs(etr[index][way]) == max_etr && etr[index][way] < 0)) {
        max_etr = abs(etr[index][way]);
        victim_way = way;
      }
    }
    return victim_way;
  }

public:
  void mj_initialize()
  {

    // put your own initialization code here
    etr.resize(LLC_SET, std::vector<int>(LLC_WAY));
    etr_clock.resize(LLC_SET, GRANULARITY);
    current_timestamp.resize(LLC_SET, 0);
    sampled_cache.reserve(LLC_SET);
    for (uint32_t set = 0; set < LLC_SET; set++) {
      if (is_sampled_set(set)) {
        int modifier = 1 << LOG2_LLC_SET;         // 4096
        int limit = 1 << LOG2_SAMPLED_CACHE_SETS; // 16
        for (int i = 0; i < limit; i++) {
          sampled_cache[set + modifier * i].resize(SAMPLED_CACHE_WAYS, SampledCacheLine());
        }
      }
    }
  }

  void mj_update(uint64_t full_addr, uint32_t set, uint32_t way, bool hit, uint64_t pc)
  {
    uint64_t pc_sig = get_pc_signature(pc, hit);
    // set = full_addr % LLC_SET;
    if (is_sampled_set(set)) {
      uint64_t sampled_cache_index = get_sampled_cache_index(full_addr);
      uint64_t sampled_cache_tag = get_sampled_cache_tag(full_addr);
      int sampled_cache_way = search_sampled_cache(sampled_cache_tag, sampled_cache_index);
      if (sampled_cache_way > -1) {
        uint64_t last_signature = sampled_cache[sampled_cache_index][sampled_cache_way].signature;
        uint64_t last_timestamp = sampled_cache[sampled_cache_index][sampled_cache_way].timestamp;
        int sample = time_elapsed(current_timestamp[set], last_timestamp);

        if (sample <= INF_RD) {
          if (rdp.count(last_signature)) // update rdp
          {
            int init = rdp[last_signature];
            // 更新 rdp 的 reuse 距离预测值
            rdp[last_signature] = temporal_difference(init, sample);
          } else {
            rdp[last_signature] = sample;
          }

          sampled_cache[sampled_cache_index][sampled_cache_way].valid = false; //?
        }
      }
      // find victim in sampled cache
      int lru_way = -1;
      int lru_rd = -1;
      for (int w = 0; w < SAMPLED_CACHE_WAYS; w++) {
        if (sampled_cache[sampled_cache_index][w].valid == false) {
          lru_way = w;
          lru_rd = INF_RD + 1;
          continue;
        }

        uint64_t last_timestamp = sampled_cache[sampled_cache_index][w].timestamp;
        int sample = time_elapsed(current_timestamp[set], last_timestamp);
        if (sample > INF_RD) {
          lru_way = w;
          lru_rd = INF_RD + 1;
          detrain(sampled_cache_index, w);
        } else if (sample > lru_rd) {
          lru_way = w;
          lru_rd = sample;
        }
      }
      detrain(sampled_cache_index, lru_way);
      // 如果 is_sampled_set，则把该块放入采样缓存
      for (int w = 0; w < SAMPLED_CACHE_WAYS; w++) {
        if (sampled_cache[sampled_cache_index][w].valid == false) {
          sampled_cache[sampled_cache_index][w].valid = true;
          sampled_cache[sampled_cache_index][w].signature = pc_sig;
          sampled_cache[sampled_cache_index][w].tag = sampled_cache_tag;
          sampled_cache[sampled_cache_index][w].timestamp = current_timestamp[set];
          break;
        }
      }

      current_timestamp[set] = increment_timestamp(current_timestamp[set]);
    }

    // 每八次访问（GRANULARITY）更新一次 etr
    if (etr_clock[set] == GRANULARITY) {
      for (int w = 0; w < LLC_WAY; w++) {
        if ((uint32_t)w != way && abs(etr[set][w]) < INF_ETR) {
          etr[set][w]--;
        }
      }
      etr_clock[set] = 0;
    }
    etr_clock[set]++;

    if (way < LLC_WAY) {
      if (!rdp.count(pc_sig)) {
        etr[set][way] = 0;

      } else {
        if (rdp[pc_sig] > MAX_RD) {
          etr[set][way] = INF_ETR;
        } else {
          etr[set][way] = rdp[pc_sig] / GRANULARITY;
        }
      }
    }
  }

  std::unordered_map<uint64_t, int> rdp;
  std::vector<int> current_timestamp;
  std::vector<std::vector<int>> etr;
  std::vector<int> etr_clock;
  std::unordered_map<uint64_t, std::vector<SampledCacheLine>> sampled_cache;
};

template <class T>
class INFMJSetAssociativeCache : public SetAssociativeCache<T>
{
  typedef SetAssociativeCache<T> Super;

public:
  INFMJSetAssociativeCache(int size, int num_ways, int debug_level = 0) : Super(size, num_ways, debug_level) {}
  // const int LLC_SET = 4096;
  // const int LLC_WAY = 96;
  // const int LOG2_LLC_SET = 12;
  const int LLC_SET = 4096 * 8;
  const int LLC_WAY = 12;
  const int LOG2_LLC_SET = 12 + 3;
  const int LOG2_BLOCK_SIZE = 6;
  const int LOG2_PAGE_SIZE = 12;

  const int HISTORY = 8;
  const int GRANULARITY = 8;

  const int INF_RD = 64;
  const int INF_ETR = 8;
  const int MAX_RD = 64;

  const int PC_SIGNATURE_BITS = 12;
  const int TIMESTAMP_BITS = 8;

  const double TEMP_DIFFERENCE = 1.0 / 16.0;
  const double FLEXMIN_PENALTY = 2.0 - 0 / 4.0;

  struct SampledCacheLine {
    bool valid;
    uint64_t signature;
    uint64_t page_signature;
    int timestamp;
    SampledCacheLine(bool v = true, uint64_t s = 0, uint64_t ps = 0, int t = 0) : valid(v), signature(s), page_signature(ps), timestamp(t) {};
  };

  uint64_t CRC_HASH(uint64_t _blockAddress)
  {
    static const unsigned long long crcPolynomial = 3988292384ULL;
    unsigned long long _returnVal = _blockAddress;
    for (unsigned int i = 0; i < 3; i++)
      _returnVal = ((_returnVal & 1) == 1) ? ((_returnVal >> 1) ^ crcPolynomial) : (_returnVal >> 1);
    return _returnVal;
  }

  uint64_t get_pc_signature(uint64_t pc, bool hit)
  {
    pc = pc << 1;
    if (hit) {
      pc = pc | 1;
    }
    pc = CRC_HASH(pc);
    pc = (pc << (64 - PC_SIGNATURE_BITS)) >> (64 - PC_SIGNATURE_BITS);
    return pc;
  }

  uint64_t get_page_signature(uint64_t pc, uint64_t addr)
  {
    uint64_t key = pc ^ (addr >> LOG2_PAGE_SIZE);
    key = CRC_HASH(key);
    return key;
  }

  int temporal_difference(int init, int sample) // soft update
  {
    if (sample > init) {
      int diff = sample - init;
      diff = diff * TEMP_DIFFERENCE;
      diff = min(1, diff);
      return min(init + diff, INF_RD);
    } else if (sample < init) {
      int diff = init - sample;
      diff = diff * TEMP_DIFFERENCE;
      diff = min(1, diff);
      return max(init - diff, 0);
    } else {
      return init;
    }
  }

  int increment_timestamp(int input)
  {
    input++;
    input = input % (1 << TIMESTAMP_BITS);
    return input;
  }

  int time_elapsed(int global, int local)
  {
    if (global >= local) {
      return global - local;
    }
    global = global + (1 << TIMESTAMP_BITS);
    return global - local;
  }

protected:
  /* @override */
  int select_victim(uint64_t index) override
  {
    // your eviction policy goes here
    int max_etr = 0;
    int victim_way = 0;
    for (int way = 0; way < LLC_WAY; way++) {
      if (abs(etr[index][way]) > max_etr || (abs(etr[index][way]) == max_etr && etr[index][way] < 0)) {
        max_etr = abs(etr[index][way]);
        victim_way = way;
      }
    }
    return victim_way;
  }

public:
  void mj_initialize()
  {
    // put your own initialization code here
    etr.resize(LLC_SET, std::vector<int>(LLC_WAY));
    etr_clock.resize(LLC_SET, GRANULARITY);
    current_timestamp.resize(LLC_SET, 0);
  }

  void mj_update(uint64_t full_addr, uint32_t set, uint32_t way, bool hit, uint64_t pc)
  {
    uint64_t pc_sig = get_pc_signature(pc, hit);
    uint64_t page_sig = get_page_signature(pc, full_addr & ((1 << 32) - 1));

    if (inf_track_cache.count(full_addr)) {
      uint64_t last_signature = inf_track_cache[full_addr].signature;
      uint64_t last_page_signature = inf_track_cache[full_addr].page_signature;
      uint64_t last_timestamp = inf_track_cache[full_addr].timestamp;
      inf_track_cache[full_addr].timestamp = current_timestamp[set];
      int sample = time_elapsed(current_timestamp[set], last_timestamp);
      if (sample <= INF_RD) {
        if (rdp.count(last_signature)) // update rdp
        {
          int init = rdp[last_signature];
          // 更新 rdp 的 reuse 距离预测值
          rdp[last_signature] = temporal_difference(init, sample);
        } else {
          rdp[last_signature] = sample;
        }
        if (page_rdp.count(last_page_signature)) {
          int init = page_rdp[last_page_signature];
          page_rdp[last_page_signature] = max(init, sample);
        } else {
          page_rdp[last_page_signature] = sample;
        }
      }

    } else {
      SampledCacheLine temp(true, pc_sig, page_sig, current_timestamp[set]);
      inf_track_cache[full_addr] = temp;
    }
    current_timestamp[set] = increment_timestamp(current_timestamp[set]);

    // 每八次访问（GRANULARITY）更新一次 etr
    if (etr_clock[set] == GRANULARITY) {
      for (int w = 0; w < LLC_WAY; w++) {
        if ((uint32_t)w != way && abs(etr[set][w]) < INF_ETR) {
          etr[set][w]--;
        }
      }
      etr_clock[set] = 0;
    }
    etr_clock[set]++;

    if (way < LLC_WAY) {
      if (page_rdp.count(page_sig)) {
        etr[set][way] = page_rdp[page_sig] / GRANULARITY;
      } else if (rdp.count(pc_sig)) {
        etr[set][way] = rdp[pc_sig] / GRANULARITY;
      } else {
        etr[set][way] = 0;
      }
    }
  }

  std::unordered_map<uint64_t, int> rdp;
  std::unordered_map<uint64_t, int> page_rdp;
  std::vector<int> current_timestamp;
  std::vector<std::vector<int>> etr;
  std::vector<int> etr_clock;
  std::unordered_map<uint64_t, SampledCacheLine> inf_track_cache;
};

uint64_t hash_index(uint64_t key, int index_len);
template <class T>
inline T square(T x)
{
  return x * x;
}

template <class T>
class GenericSatCounter
{
public:
  /** The default constructor should never be used. */
  GenericSatCounter() = delete;

  /**
   * Constructor for the counter. The explicit keyword is used to make
   * sure the user does not assign a number to the counter thinking it
   * will be used as a counter value when it is in fact used as the number
   * of bits.
   *
   * @param bits How many bits the counter will have.
   * @param initial_val Starting value for the counter.
   *
   * @ingroup api_sat_counter
   */
  explicit GenericSatCounter(unsigned bits, T initial_val = 0) : initialVal(initial_val), maxVal((1ULL << bits) - 1), counter(initial_val)
  {
    // fatal_if(bits > 8*sizeof(T),
    //  "Number of bits exceeds counter size");
    // fatal_if(initial_val > maxVal,
    //  "Saturating counter's initial value exceeds max value.");
    assert(bits <= 8 * sizeof(T));
    assert(initial_val <= maxVal);
  }

  /**
   * Copy constructor.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter(const GenericSatCounter& other) : initialVal(other.initialVal), maxVal(other.maxVal), counter(other.counter) {}

  /**
   * Copy assignment.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator=(const GenericSatCounter& other)
  {
    if (this != &other) {
      GenericSatCounter temp(other);
      this->swap(temp);
    }
    return *this;
  }

  /**
   * Move constructor.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter(GenericSatCounter&& other)
  {
    initialVal = other.initialVal;
    maxVal = other.maxVal;
    counter = other.counter;
    GenericSatCounter temp(0);
    other.swap(temp);
  }

  /**
   * Move assignment.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator=(GenericSatCounter&& other)
  {
    if (this != &other) {
      initialVal = other.initialVal;
      maxVal = other.maxVal;
      counter = other.counter;
      GenericSatCounter temp(0);
      other.swap(temp);
    }
    return *this;
  }

  /**
   * Swap the contents of every member of the class. Used for the default
   * copy-assignment created by the compiler.
   *
   * @param other The other object to swap contents with.
   *
   * @ingroup api_sat_counter
   */
  void swap(GenericSatCounter& other)
  {
    std::swap(initialVal, other.initialVal);
    std::swap(maxVal, other.maxVal);
    std::swap(counter, other.counter);
  }

  /**
   * Pre-increment operator.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator++()
  {
    if (counter < maxVal) {
      ++counter;
    }
    return *this;
  }

  /**
   * Post-increment operator.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter operator++(int)
  {
    GenericSatCounter old_counter = *this;
    ++*this;
    return old_counter;
  }

  /**
   * Pre-decrement operator.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator--()
  {
    if (counter > 0) {
      --counter;
    }
    return *this;
  }

  /**
   * Post-decrement operator.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter operator--(int)
  {
    GenericSatCounter old_counter = *this;
    --*this;
    return old_counter;
  }

  /**
   * Shift-right-assignment.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator>>=(const int& shift)
  {
    assert(shift >= 0);
    this->counter >>= shift;
    return *this;
  }

  /**
   * Shift-left-assignment.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator<<=(const int& shift)
  {
    assert(shift >= 0);
    this->counter <<= shift;
    if (this->counter > maxVal) {
      this->counter = maxVal;
    }
    return *this;
  }

  /**
   * Add-assignment.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator+=(const long long& value)
  {
    if (value >= 0) {
      if (maxVal - this->counter >= value) {
        this->counter += value;
      } else {
        this->counter = maxVal;
      }
    } else {
      *this -= -value;
    }
    return *this;
  }

  /**
   * Subtract-assignment.
   *
   * @ingroup api_sat_counter
   */
  GenericSatCounter& operator-=(const long long& value)
  {
    if (value >= 0) {
      if (this->counter > value) {
        this->counter -= value;
      } else {
        this->counter = 0;
      }
    } else {
      *this += -value;
    }
    return *this;
  }

  /**
   * Read the counter's value.
   *
   * @ingroup api_sat_counter
   */
  operator T() const { return counter; }

  /**
   * Reset the counter to its initial value.
   *
   * @ingroup api_sat_counter
   */
  void reset() { counter = initialVal; }

  /**
   * Calculate saturation percentile of the current counter's value
   * with regard to its maximum possible value.
   *
   * @return A value between 0.0 and 1.0 to indicate which percentile of
   *         the maximum value the current value is.
   *
   * @ingroup api_sat_counter
   */
  double calcSaturation() const { return (double)counter / maxVal; }

  /**
   * Whether the counter has achieved its maximum value or not.
   *
   * @return True if the counter saturated.
   *
   * @ingroup api_sat_counter
   */
  bool isSaturated() const { return counter == maxVal; }

  /**
   * Saturate the counter.
   *
   * @return The value added to the counter to reach saturation.
   *
   * @ingroup api_sat_counter
   */
  T saturate()
  {
    const T diff = maxVal - counter;
    counter = maxVal;
    return diff;
  }

private:
  T initialVal;
  T maxVal;
  T counter;
};

/** @ingroup api_sat_counter
 *  @{
 */
typedef GenericSatCounter<uint8_t> SatCounter8;
typedef GenericSatCounter<uint16_t> SatCounter16;
typedef GenericSatCounter<uint32_t> SatCounter32;
typedef GenericSatCounter<uint64_t> SatCounter64;

class TaggedEntry
{
public:
  TaggedEntry() : _valid(false), _secure(false), _tag((uint64_t)-1) {}
  ~TaggedEntry() = default;

  /**
   * Checks if the entry is valid.
   *
   * @return True if the entry is valid.
   */
  virtual bool isValid() const { return _valid; }

  /**
   * Check if this block holds data from the secure memory space.
   *
   * @return True if the block holds data from the secure memory space.
   */
  bool isSecure() const { return _secure; }

  /**
   * Get tag associated to this block.
   *
   * @return The tag value.
   */
  virtual uint64_t getTag() const { return _tag; }

  /**
   * Checks if the given tag information corresponds to this entry's.
   *
   * @param tag The tag value to compare to.
   * @param is_secure Whether secure bit is set.
   * @return True if the tag information match this entry's.
   */
  virtual bool matchTag(uint64_t tag, bool is_secure) const { return isValid() && (getTag() == tag) && (isSecure() == is_secure); }

  /**
   * Insert the block by assigning it a tag and marking it valid. Touches
   * block if it hadn't been touched previously.
   *
   * @param tag The tag value.
   */
  virtual void insert(const uint64_t tag, const bool is_secure)
  {
    setValid();
    setTag(tag);
    if (is_secure) {
      setSecure();
    }
  }

  /** Invalidate the block. Its contents are no longer valid. */
  virtual void invalidate()
  {
    _valid = false;
    setTag((uint64_t)-1);
    clearSecure();
  }

  // std::string
  // print() const override
  // {
  //     return csprintf("tag: %#x secure: %d valid: %d | %s", getTag(),
  //         isSecure(), isValid(), ReplaceableEntry::print());
  // }

protected:
  /**
   * Set tag associated to this block.
   *
   * @param tag The tag value.
   */
  virtual void setTag(uint64_t tag) { _tag = tag; }

  /** Set secure bit. */
  virtual void setSecure() { _secure = true; }

  /** Set valid bit. The block must be invalid beforehand. */
  virtual void setValid()
  {
    assert(!isValid());
    _valid = true;
  }

private:
  /**
   * Valid bit. The contents of this entry are only valid if this bit is set.
   * @sa invalidate()
   * @sa insert()
   */
  bool _valid;

  /**
   * Secure bit. Marks whether this entry refers to an address in the secure
   * memory space. Must always be modified along with the tag.
   */
  bool _secure;

  /** The entry's tag. */
  uint64_t _tag;

  /** Clear secure bit. Should be only used by the invalidation function. */
  void clearSecure() { _secure = false; }
};

#endif /* BAKSHALIPOUR_FRAMEWORK */
