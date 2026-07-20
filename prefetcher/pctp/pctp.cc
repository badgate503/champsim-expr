#include "pctp.h"

#include <cassert>
#include <utility>

int pctp::issue_metatable(pctpMetaTable* metaTable, uint64_t lookup, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    pctpMetaTableEntry* candidate = metaTable->find(lookup);
    meta_table_lookups++;
    if (candidate == nullptr)
      break;
    meta_table_hits++;
    if (candidate->correlated_addr != 0) {
      if (!isAlreadyInQueue(addresses, candidate->correlated_addr)) {
        addresses.push_back(candidate->correlated_addr);
        meta_table_issued_prefetches++;
        meta_table_prefetches.insert(candidate->correlated_addr);
        issued++;
      }
      lookup = candidate->correlated_addr;
    }
  }
  return issued;
}

uint32_t pctp::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                        uint32_t metadata_in, std::string latepf)
{
  if (meta_table_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    meta_table_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    meta_table_accurate_prefetches++;
  }
  if (PCM_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) && latepf != "NO") {
    PCM_late_prefetches++;
    PCM_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (cache_hit && PCM_filled_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_useful_prefetches++;
    PCM_filled_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
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
    std::deque<uint64_t> temp(1, 0);
    pcTable->insert(pc, temp);
    pc_entry = pcTable->find(pc);
  }
  pcTable->set_mru(pc);

  uint64_t lookup_key = block_addr;
  pctpMetaTableEntry* metadata = metaTable->find(lookup_key);
  if (metadata) {
    metaTable->set_mru(lookup_key);
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

    pctpMetaTableEntry* last_meta = metaTable->find(insert_key);

    if (last_meta) {
      bool matched = false;
      if (last_meta->correlated_addr == block_addr) {
        matched = true;
      } else {
        uint64_t victim_addr = last_meta->correlated_addr;
        pctpMetaTableEntry temp_entry(block_addr);
        metaTable->insert(insert_key, temp_entry);
      }
    } else {
      pctpMetaTableEntry temp_entry(block_addr);
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
    if (pc_entry->data.size() > 1)
      pc_entry->data.pop_back();
  }

  // PC triggered prefetches
  if (last_addr == 0 || last_addr == block_addr) //
  // if (last_addr == block_addr) // pctp2
  {
    if (!cache_hit && PCQ.size() > 0) {
      uint64_t triggerIP = 0;
      triggerIP = PCQ.back();

      if (triggerIP) {
        triggerIP = hash_xor(triggerIP);
#ifdef INF_PAT
        pc_meta_table[triggerIP] = block_addr;
#else
        auto victim = pc_meta_table->insert(triggerIP, block_addr);
        if (!victim.valid || victim.key != triggerIP)
          pc_meta_table->set_default(triggerIP);
        else if (victim.valid && victim.key == triggerIP && victim.data == block_addr)
          pc_meta_table->touch(triggerIP);
#endif
      }
    }
  }

  uint64_t lookupPC = hash_xor(pc);
#ifdef INF_PAT
  if (pc_meta_table.find(lookupPC) != pc_meta_table.end() && !isAlreadyInQueue(pref_addr, pc_meta_table[lookupPC])) {
    pref_addr.push_back(pc_meta_table[lookupPC]);
    PCM_issued_prefetches.insert(pc_meta_table[lookupPC]);
  }
#else
  auto PCM_entry = pc_meta_table->find(lookupPC);
  if (PCM_entry && !isAlreadyInQueue(pref_addr, PCM_entry->data)) {
    // pc_meta_table->touch(lookupPC);
    pref_addr.push_back(PCM_entry->data);
    PCM_issued_prefetches.insert(PCM_entry->data);
  }
#endif

#if ONLY_TRIGGER_ON_MISS
  if (!cache_hit)
#endif
  {
    bool already_in_queue = false;
    for (const auto& p : PCQ) {
      if (p == pc) {
        already_in_queue = true;
        break;
      }
    }
    if (!already_in_queue) {
      if (PCQ.size() < PCQ_SIZE) {
        PCQ.push_front(pc);
      } else {
        PCQ.pop_back();
        PCQ.push_front(pc);
      }
    }
  }

  for (int i = 0; i < pref_addr.size(); i++) {
    const bool success = prefetch_line({pref_addr[i] << LOG2_BLOCK_SIZE}, true, 0);
  }
  return metadata_in;
}

uint32_t pctp::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  meta_table_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  if (prefetch && PCM_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_filled_prefetches.insert(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    PCM_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (PCM_filled_prefetches.count(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_useless_prefetches++;
    PCM_filled_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  return metadata_in;
}

void pctp::prefetcher_final_stats()
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

  cout << "PCM_late_prefetches " << PCM_late_prefetches << endl;  
  cout << "PCM_useful_prefetches " << PCM_useful_prefetches << endl;
  cout << "PCM_useless_prefetches " << PCM_useless_prefetches << endl;
  uint64_t PCM_accurate_prefetches = PCM_late_prefetches + PCM_useful_prefetches;
  uint64_t PCM_total_prefetches = PCM_late_prefetches + PCM_useful_prefetches + PCM_useless_prefetches;
  cout << "PCM_accuracy " << (PCM_total_prefetches ? (double)PCM_accurate_prefetches / PCM_total_prefetches : 0) << endl;
  cout << "PCM_laterate " << (PCM_accurate_prefetches ? (double)PCM_late_prefetches / PCM_accurate_prefetches : 0) << endl;
}

void pctp::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{

}

void pctp::prefetcher_cycle_operate() {}

bool pctpMetaTable::insert(uint64_t key, const pctpMetaTableEntry& data)
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
    ret = true;
  }
  return ret;
}