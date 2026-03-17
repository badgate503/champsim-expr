#include "conftp.h"

#include <cassert>
#include <utility>

int conftp::issue_metatable(conftpMetaTable* metaTable, uint64_t lookup, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    conftpMetaTableEntry* candidate = metaTable->find(lookup);
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

uint32_t conftp::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
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
  if (conflict_table_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) && latepf != "NO") {
    conflict_table_late_prefetches++;
    conflict_table_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (cache_hit && conflict_table_filled_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)){
    conflict_table_useful_prefetches++;
    conflict_table_filled_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
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
    std::deque<uint64_t> temp(2, 0);
    pcTable->insert(pc, temp);
    pc_entry = pcTable->find(pc);
  }
  pcTable->set_mru(pc);

  uint64_t lookup_key = block_addr;
  conftpMetaTableEntry* metadata = metaTable->find(lookup_key);
  if (metadata) {
    metaTable->set_mru(lookup_key);
    if (!metadata->used) {
      metadata->used = true;
    }
  }

  // 2.issue: metadata table
  uint64_t conflictTableKey = last_addr ^ (block_addr << 5) ^ (block_addr >> 7);
  conflictTableKey ^= conflictTableKey >> 16;
  conflict_table_lookups++;
#ifdef INF_CONFLICT_TABLE
  if (conflictTable.find(conflictTableKey) != conflictTable.end()) {
    pref_addr.push_back(conflictTable[conflictTableKey]);
    conflict_table_hits++;
    // conflict_table_issued_prefetches++;
    conflict_table_issued_prefetches.insert(conflictTable[conflictTableKey]);
#ifdef ELABORATE_LOG
    logfile << std::dec << llc_cache->current_cycle() << " ISSUE CT " << std::hex << ip << " " << (conflictTableKey) << " " << (conflictTable[conflictTableKey])
            << std::endl;
#endif
  }
#else
  auto conflict_table_entry = conflictTable->find(conflictTableKey);
  if (conflict_table_entry) {
    // conflictTable->touch(conflictTableKey);
    pref_addr.push_back(conflict_table_entry->data);
    conflict_table_hits++;
    // conflict_table_issued_prefetches++;
    conflict_table_issued_prefetches.insert(conflict_table_entry->data);
#ifdef ELABORATE_LOG
    logfile << std::dec << llc_cache->current_cycle() << " ISSUE CT " << std::hex << ip << " " << (conflictTableKey) << " " << (conflict_table_entry->data)
            << std::endl;
#endif
  }
#endif
  int issued_by_metatable = issue_metatable(metaTable, lookup_key, pref_addr);

  // 3.update
  // 3.1 update the metaTable
  if (last_addr != 0 && last_addr != block_addr) {
    uint64_t insert_key = last_addr;

    conftpMetaTableEntry* last_meta = metaTable->find(insert_key);

    if (last_meta) {
      bool matched = false;
      if (last_meta->correlated_addr == block_addr) {
        matched = true;
      } else {
        uint64_t last_last_addr = 0;
        if (pc_entry->data.size() > 1)
          last_last_addr = pc_entry->data[1];
        if (last_last_addr) {
          conflictTableKey = last_last_addr ^ (last_addr << 5) ^ (last_addr >> 7);
          conflictTableKey ^= conflictTableKey >> 16;

#ifdef INF_CONFLICT_TABLE
          if (conflictTable.count(conflictTableKey) && conflictTable[conflictTableKey] != block_addr) {
            metaTable->insert(last_addr, {conflictTable[conflictTableKey]});
          }
          conflictTable[conflictTableKey] = block_addr;
#else
          conflict_table_entry = conflictTable->find(conflictTableKey);
          if (conflict_table_entry && conflict_table_entry->data != block_addr) {
            metaTable->insert(last_addr, {conflict_table_entry->data});
          }
          if (conflict_table_entry && conflict_table_entry->data == block_addr) {
            conflictTable->touch(conflictTableKey);
          } else {
            conflictTable->insert(conflictTableKey, block_addr);
            conflictTable->set_default(conflictTableKey);
          }
#endif
        }

        // uint64_t victim_addr = last_meta->correlated_addr;
        // conftpMetaTableEntry temp_entry(block_addr);
        // metaTable->insert(insert_key, temp_entry);
      }
    } else {
      conftpMetaTableEntry temp_entry(block_addr);
      if (!metaTable->insert(insert_key, temp_entry)) {
        numEntriesinTable++;
      }
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
    if (pc_entry->data.size() > 2)
      pc_entry->data.pop_back();
  }

  for (int i = 0; i < pref_addr.size(); i++) {
    const bool success = prefetch_line({pref_addr[i] << LOG2_BLOCK_SIZE}, true, 0);
  }
  return metadata_in;
}

uint32_t conftp::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  meta_table_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  if (conflict_table_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)){
    conflict_table_filled_prefetches.insert(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    conflict_table_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (conflict_table_filled_prefetches.count(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)){
    conflict_table_useless_prefetches++;
    conflict_table_filled_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  return metadata_in;
}

void conftp::prefetcher_final_stats()
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

  cout << "CT_lookups " << conflict_table_lookups << endl;
  cout << "CT_hits " << conflict_table_hits << endl;
  cout << "CT_hitrate " << (conflict_table_lookups ? (double)conflict_table_hits / conflict_table_lookups : 0) << endl;
  cout << "CT_late_prefetches " << conflict_table_late_prefetches << endl;
  cout << "CT_useful_prefetches " << conflict_table_useful_prefetches << endl;
  cout << "CT_useless_prefetches " << conflict_table_useless_prefetches << endl;
  uint64_t CT_accurate_prefetches = conflict_table_late_prefetches + conflict_table_useful_prefetches;
  uint64_t CT_total_prefetches = conflict_table_late_prefetches + conflict_table_useful_prefetches + conflict_table_useless_prefetches;
  cout << "CT_accuracy " << (CT_total_prefetches ? (double)CT_accurate_prefetches / CT_total_prefetches : 0) << endl;
}

void conftp::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
#ifdef ELABORATE_LOG
  logfile << std::dec << llc_cache->current_cycle() << " MSHRPFHIT " << std::hex << (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << " " << ip << std::dec
          << std::endl;
#endif
}

void conftp::prefetcher_cycle_operate() {}

bool conftpMetaTable::insert(uint64_t key, const conftpMetaTableEntry& data)
{
  reverse_metatable[data.correlated_addr].insert(key);
  Entry victim_entry = Super::insert(key, data);
  Super::set_mru(key);
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
