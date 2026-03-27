#include "drtp.h"

#include <cassert>
#include <utility>

int drtp::issue_metatable(drtpMetaTable* metaTable, uint64_t lookup, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    drtpMetaTableEntry* candidate = metaTable->find(lookup);
    meta_table_lookups++;
    if (candidate == nullptr)
      break;
    meta_table_hits++;
    if (candidate->correlated_addr != 0) {
      if (!isAlreadyInQueue(addresses, candidate->correlated_addr)) {
        addresses.push_back(candidate->correlated_addr);
        meta_table_issued_prefetches++;
        meta_table_prefetches.insert(candidate->correlated_addr);
#ifdef ELABORATE_LOG
        logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlated_addr)
                << std::endl;
#endif
        issued++;
      }
      lookup = candidate->correlated_addr;
    }
  }
  return issued;
}

uint32_t drtp::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                            uint32_t metadata_in, std::string latepf)
{
#ifdef ELABORATE_LOG
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

  uint64_t pc = ip.to<uint64_t>();
  uint64_t block_addr = addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
  vector<uint64_t> pref_addr;

  if (pc == 0) {
    return metadata_in;
  }

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
  lookup_key ^=
      (pc_entry->data[0] << 5) ^ (pc_entry->data[0] >> 7) ^ (pc_entry->data[1] << 11) ^ (pc_entry->data[1] >> 13) ^ (pc_entry->data[2] << 17) ^ (pc_entry->data[2] >> 19);
  lookup_key ^= lookup_key >> 16;
#endif

  drtpMetaTableEntry* metadata = metaTable->find(lookup_key);
  if (metadata) {
    // metaTable->set_mru(lookup_key);
    if (!metadata->used) {
      metadata->used = true;
    }
  }

  // 2.issue: metadata table
  int issued_by_metatable = issue_metatable(metaTable, lookup_key, pref_addr);

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
    lookup_key ^=
        (pc_entry->data[1] << 5) ^ (pc_entry->data[1] >> 7) ^ (pc_entry->data[2] << 11) ^ (pc_entry->data[2] >> 13) ^ (pc_entry->data[3] << 17) ^ (pc_entry->data[3] >> 19);
    insert_key ^= insert_key >> 16;
#endif

    drtpMetaTableEntry* last_meta = metaTable->find(insert_key);

    if (last_meta) {
      bool matched = false;
      if (last_meta->correlated_addr == block_addr) {
        matched = true;
        metaTable->touch(insert_key);
      } else {
        uint64_t victim_addr = last_meta->correlated_addr;
        drtpMetaTableEntry temp_entry(block_addr);
#ifdef NOMD_WHEN_HIT
        if (!cache_hit)
          metaTable->insert(insert_key, temp_entry, 1);
#else
        metaTable->insert(insert_key, temp_entry);
#endif
      }
    } else {
      drtpMetaTableEntry temp_entry(block_addr);
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

uint32_t drtp::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  meta_table_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  return metadata_in;
}

void drtp::prefetcher_final_stats()
{
#ifdef ELABORATE_LOG
  logfile.close();
#endif
  cout << "MT_lookups " << meta_table_lookups << endl;
  cout << "MT_hits " << meta_table_hits << endl;
  cout << "MT_hitrate " << (meta_table_lookups ? (double)meta_table_hits / meta_table_lookups : 0) << endl;
  cout << "MT_issuedpf " << meta_table_issued_prefetches << endl;
  cout << "MT_accuratepf " << meta_table_accurate_prefetches << endl;
  cout << "MT_accuracy " << (meta_table_issued_prefetches ? (double)meta_table_accurate_prefetches / meta_table_issued_prefetches : 0) << endl;
}

void drtp::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
#ifdef ELABORATE_LOG
  logfile << std::dec << llc_cache->current_cycle() << " MSHRPFHIT " << std::hex << (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << " " << ip << std::dec
          << std::endl;
#endif
}

void drtp::prefetcher_cycle_operate() {}

bool drtpMetaTable::insert(uint64_t key, const drtpMetaTableEntry& data)
{
  reverse_metatable[data.correlated_addr].insert(key);
  Entry victim_entry = Super::insert(key, data);
  // Super::set_mru(key);
#ifdef TOUCH_TRIGGER
  if (victim_entry.key != key)  
    set_default(key);
  else
    touch(key);
#else
  set_default(key);
#endif
  uint64_t index = key % this->num_sets;
  uint64_t tag = key / this->num_sets;
  // int way = this->cams[index][tag];
  bool ret = false;
  if (victim_entry.valid) {
    reverse_metatable[victim_entry.data.correlated_addr].erase(victim_entry.key);
#ifdef ELABORATE_LOG
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
#ifdef ELABORATE_LOG
  prefetcher->logfile << std::dec << prefetcher->llc_cache->current_cycle() << " ADD " << std::hex << key << " " << data.correlated_addr << std::endl;
#endif
  return ret;
}
