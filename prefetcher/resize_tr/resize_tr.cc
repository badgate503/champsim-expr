#include "resize_tr.h"

#include <cassert>
#include <utility>

int resize_tr::issue_metatable(resize_trMetaTable* metaTable, uint64_t pc, uint64_t lookup, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    resize_trMetaTableEntry* candidate = metaTable->find(lookup);
    meta_table_lookups++;
    if (candidate == nullptr)
      break;
    meta_table_hits++;
    if (candidate->correlated_addr != 0) {
      if (!isAlreadyInQueue(addresses, candidate->correlated_addr)) {
        addresses.push_back(candidate->correlated_addr);
        meta_table_issued_prefetches++;
        num_issued_prefetch++;
        meta_table_prefetches.insert(candidate->correlated_addr);
        issued++;
#ifdef MISS_CLASS_LOG
    logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlated_addr) << std::endl;
#endif
      }
      lookup = candidate->correlated_addr;
    }
  }
  return issued;
}

uint32_t resize_tr::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                            uint32_t metadata_in, std::string latepf)
{
#ifdef MISS_CLASS_LOG
  if (!warmup_complete && !llc_cache->warmup) {
    warmup_complete = true;
    logfile << "WARMUP COMPLETE" << std::endl;
  }

  if (!cache_hit) { // L2 CACHE MISS
    if (ip.to<uint64_t>() != 0) {
      champsim::block_number pf_addr{addr};
      // std::ofstream ofs(out_file, std::ios::app);  // append mode
      uint64_t last_addr = get_last(ip.to<uint64_t>());
      std::set<uint64_t> triggers = get_triggers(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);

      logfile << std::dec << llc_cache->current_cycle() << " MISS " << latepf << " " << std::hex << pf_addr << " " << ip << " " << last_addr;
      for (uint64_t t : triggers) {
        logfile << " " << t;
      }
      logfile << std::endl;
    }
  } else {
    if (ip.to<uint64_t>() != 0) {
      champsim::block_number pf_addr{addr};
      // std::ofstream ofs(out_file, std::ios::app);  // append mode
      uint64_t last_addr = get_last(ip.to<uint64_t>());
      std::set<uint64_t> triggers = get_triggers(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);

      logfile << std::dec << llc_cache->current_cycle() << " HIT " << std::hex << pf_addr << " " << ip << " " << last_addr;
      for (uint64_t t : triggers) {
        logfile << " " << t;
      }
      logfile << std::endl;
    }
  }
#endif
  


  if (meta_table_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    meta_table_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    meta_table_accurate_prefetches++;
  }
  if (useful_prefetch){
    num_useful_prefetch++;
  }
  

  // should resize

  uint64_t pc = ip.to<uint64_t>();
  uint64_t block_addr = addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
  vector<uint64_t> pref_addr;

  /* tr resize policy */
  if (ip.to<uint64_t>() == 0 || type == access_type::WRITE) {
    SD->TryCacheHit(block_addr); // only check cache hit for set dueling
    return metadata_in;         // Ignore if no IP info
  }

  
  /* tr resize policy */

  // 1.search
  uint64_t last_addr = 0;
  auto pc_entry = pcTable->find(pc);
  if (pc_entry) {
    last_addr = pc_entry->data.front();
  } else {
#ifdef BASE_TRIGGER_NUM
    std::deque<uint64_t> temp(BASE_TRIGGER_NUM, 0);
    pcTable->insert(pc, temp);
#else
    std::deque<uint64_t> temp(1, 0);
    pcTable->insert(pc, temp);
#endif
    pc_entry = pcTable->find(pc);
  }
  pcTable->set_mru(pc);

  uint64_t lookup_key = block_addr;
#if BASE_TRIGGER_NUM == 2
  lookup_key ^= (pc_entry->data[0] << 5) ^ (pc_entry->data[0] >> 7);
  lookup_key ^= lookup_key >> 16;
#elif BASE_TRIGGER_NUM == 3
  lookup_key ^= (pc_entry->data[0] << 5) ^ (pc_entry->data[0] >> 7) ^ (pc_entry->data[1] << 11) ^ (pc_entry->data[1] >> 13);
  lookup_key ^= lookup_key >> 16;
#elif BASE_TRIGGER_NUM == 4
  lookup_key ^= (pc_entry->data[0] << 5) ^ (pc_entry->data[0] >> 7) ^ (pc_entry->data[1] << 11) ^ (pc_entry->data[1] >> 13) ^ (pc_entry->data[2] << 17)
                ^ (pc_entry->data[2] >> 19);
  lookup_key ^= lookup_key >> 16;
#endif

  resize_trMetaTableEntry* metadata = metaTable->find(lookup_key);
  if (metadata) {
    // metaTable->set_mru(lookup_key);
    if (!metadata->used) {
      metadata->used = true;
    }
  }

  // 2.issue: metadata table
  int issued_by_metatable = issue_metatable(metaTable, pc, lookup_key, pref_addr);

  // 3.update
  // 3.1 update the metaTable
  if (last_addr != 0 && last_addr != block_addr) {
    uint64_t insert_key = last_addr;
#if BASE_TRIGGER_NUM == 2
    insert_key ^= (pc_entry->data[1] << 5) ^ (pc_entry->data[1] >> 7);
    insert_key ^= insert_key >> 16;
#elif BASE_TRIGGER_NUM == 3
    insert_key ^= (pc_entry->data[1] << 5) ^ (pc_entry->data[1] >> 7) ^ (pc_entry->data[2] << 11) ^ (pc_entry->data[2] >> 13);
    insert_key ^= insert_key >> 16;
#elif BASE_TRIGGER_NUM == 4
    lookup_key ^= (pc_entry->data[1] << 5) ^ (pc_entry->data[1] >> 7) ^ (pc_entry->data[2] << 11) ^ (pc_entry->data[2] >> 13) ^ (pc_entry->data[3] << 17)
                  ^ (pc_entry->data[3] >> 19);
    insert_key ^= insert_key >> 16;
#endif

    resize_trMetaTableEntry* last_meta = metaTable->find(insert_key);

    if (last_meta) {
      bool matched = false;
      if (last_meta->correlated_addr == block_addr) {
        matched = true;
        metaTable->touch(insert_key);
      } else {
        uint64_t victim_addr = last_meta->correlated_addr;
        resize_trMetaTableEntry temp_entry(block_addr);
#ifdef NOMD_WHEN_HIT
        if (!cache_hit)
          metaTable->insert(insert_key, temp_entry, 1);
#else
        metaTable->insert(insert_key, temp_entry);
#endif
      }
    } else {
      resize_trMetaTableEntry temp_entry(block_addr);
#ifdef NOMD_WHEN_HIT
      if (!cache_hit)
        if (!metaTable->insert(insert_key, temp_entry, 1))
          numEntriesinTable++;
#else
      if (!metaTable->insert(insert_key, temp_entry)) {
        numEntriesinTable++;
      }
#endif
    }

  }
  SD->TryCacheHit(block_addr); // just mantain the size dueller
  if (metadata) {
    SD->TryMarkovHit(block_addr);
  }

  if (global_timestamp > 50000000) {
    /* 找到 Hits 计数器里最大的 Hits[m] */
    int target_size = SD->Duel();
    uint64_t target_score = SD->dueller_counters[target_size];
    uint64_t current_score = SD->dueller_counters[current_partition];
    /* 若当前分区大小 cur_size 对应的 Hits[cur_size] 小于 Hits[m] 的 4/5，则调整分区，且目标分区大小为 m （Cache Line 数） */
    if (target_size != (current_partition) && target_score > current_score * 1.25) {
      current_partition = target_size;
      metadata_resize(current_partition);
      llc_cache->set_available_ways(16 - current_partition);
    }

    global_timestamp = 0;
    SD->ResetCounters();
  }
  global_timestamp++;

  // 3.2 update the pcTable
  bool already_exist = false;
  for (auto& a : pc_entry->data) {
    if (a == block_addr) {
      already_exist = true;
      break;
    }
  }
  if (!already_exist) {
    pc_entry->data.push_front(block_addr);
#ifdef BASE_TRIGGER_NUM
    if (pc_entry->data.size() > BASE_TRIGGER_NUM)
      pc_entry->data.pop_back();
#else
    if (pc_entry->data.size() > 1)
      pc_entry->data.pop_back();
#endif
  }

  for (int i = 0; i < pref_addr.size(); i++) {
    const bool success = prefetch_line({pref_addr[i] << LOG2_BLOCK_SIZE}, true, 0);
  }
  return metadata_in;
}

uint32_t resize_tr::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  meta_table_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  return metadata_in;
}

void resize_tr::prefetcher_final_stats()
{
#ifdef MISS_CLASS_LOG
  logfile.close();
#endif
  cout << "MT_lookups " << meta_table_lookups << endl;
  cout << "MT_hits " << meta_table_hits << endl;
  cout << "MT_hitrate " << (meta_table_lookups ? (double)meta_table_hits / meta_table_lookups : 0) << endl;
  cout << "MT_issuedpf " << meta_table_issued_prefetches << endl;
  cout << "MT_accuratepf " << meta_table_accurate_prefetches << endl;
  cout << "MT_accuracy " << (meta_table_issued_prefetches ? (double)meta_table_accurate_prefetches / meta_table_issued_prefetches : 0) << endl;

  cout << "Resize_L3Hit_rate " << llc_hit_rate << endl;
  cout << "Resize_UPF_rate " << useful_prefetch_rate << endl;
  cout << "Resize_Score " << resize_score << endl;
  cout << "Resize_WayForCache_" << waysForCache << endl;
}

void resize_tr::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
#ifdef MISS_CLASS_LOG
  logfile << std::dec << llc_cache->current_cycle() << " MSHRPFHIT " << std::hex << (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << " " << ip << std::dec
          << std::endl;
#endif
}

void resize_tr::prefetcher_cycle_operate() {}

bool resize_trMetaTable::insert(uint64_t key, const resize_trMetaTableEntry& data)
{
  reverse_metatable[data.correlated_addr].insert(key);
  Entry victim_entry = Super::insert(key, data);
  // Super::set_mru(key);
    set_default(key);

  uint64_t index = key % this->num_sets;
  uint64_t tag = key / this->num_sets;
  // int way = this->cams[index][tag];
  bool ret = false;
  if (victim_entry.valid) {
    reverse_metatable[victim_entry.data.correlated_addr].erase(victim_entry.key);
#ifdef MISS_CLASS_LOG
    std::string reason;
    if (victim_entry.tag != tag) {
      reason = "CAPACITY";
    } else {
      reason = "CONFLICT";
    }
    prefetcher->logfile << std::dec << prefetcher->llc_cache->current_cycle() << " EVICT " << reason << " " << std::hex << victim_entry.key << " "
                        << victim_entry.data.correlated_addr << std::endl;
#endif
    ret = true;
  }
#ifdef MISS_CLASS_LOG
  prefetcher->logfile << std::dec << prefetcher->llc_cache->current_cycle() << " ADD " << std::hex << key << " " << data.correlated_addr << std::endl;
#endif
  return ret;
}

bool resize_trMetaTable::insert(uint64_t key, const resize_trMetaTableEntry& data, uint64_t rrpv_value)
{
  reverse_metatable[data.correlated_addr].insert(key);
  Entry victim_entry = Super::insert(key, data);
  // Super::set_mru(key);
    set_rrpv(key, rrpv_value);

  uint64_t index = key % this->num_sets;
  uint64_t tag = key / this->num_sets;
  // int way = this->cams[index][tag];
  bool ret = false;
  if (victim_entry.valid) {
    reverse_metatable[victim_entry.data.correlated_addr].erase(victim_entry.key);
#ifdef MISS_CLASS_LOG
    std::string reason;
    if (victim_entry.tag != tag) {
      reason = "CAPACITY";
    } else {
      reason = "CONFLICT";
    }
    prefetcher->logfile << std::dec << prefetcher->llc_cache->current_cycle() << " EVICT " << reason << " " << std::hex << victim_entry.key << " "
                        << victim_entry.data.correlated_addr << std::endl;
#endif
    ret = true;
  }
#ifdef MISS_CLASS_LOG
  prefetcher->logfile << std::dec << prefetcher->llc_cache->current_cycle() << " ADD " << std::hex << key << " " << data.correlated_addr << std::endl;
#endif
  return ret;
}
