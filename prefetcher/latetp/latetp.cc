#include "latetp.h"

#include <cassert>
#include <utility>

int latetp::issue_metatable(latetpMetaTable* metaTable, uint64_t lookup, uint64_t degree, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < degree; i++) {
    latetpMetaTableEntry* candidate = metaTable->find(lookup);
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

uint32_t latetp::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
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

  uint64_t pc = ip.to<uint64_t>();
  uint64_t block_addr = addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
  vector<uint64_t> pref_addr;

  if (pc == 0) {
    return metadata_in;
  }

  epoch_demand++;
  if (useful_prefetch){
    accurate_prefetch_num++;
  }
  unused_prefetches.erase(block_addr);
#ifdef DYNAMIC_GLOBAL_DEGREE
  if (epoch_demand >= 1024){
    tune_global_degree();
  }
#endif

  if (meta_table_prefetches.count(block_addr)) {
    meta_table_prefetches.erase(block_addr);
    meta_table_accurate_prefetches++;
  }



  // 1.search
  uint64_t last_addr = 0;
  auto pc_entry = pcTable->find(pc);
  if (pc_entry) {
    // last_addr = pc_entry->data.front();
    // last_addr = pc_entry->data.addrHistory.size() > pc_entry->data.lookahead ? pc_entry->data.addrHistory[pc_entry->data.lookahead] : pc_entry->data.addrHistory.back();
    last_addr = pc_entry->data.addrHistory.size() > pc_entry->data.lookahead ? pc_entry->data.addrHistory[pc_entry->data.lookahead] : 0;
  } else {
    PCTableEntry temp;
    pcTable->insert(pc, temp);
    pc_entry = pcTable->find(pc);
  }
  pcTable->set_mru(pc);

  if (!cache_hit) {
    pc_entry->data.missCount += 1;
  } else if (useful_prefetch) {
    pc_entry->data.usefulPrefetchCount += 1;
#ifdef DYNAMIC_LOCAL_DEGREE
    if (pc_entry->data.usefulPrefetchCount >= 128 || pc_entry->data.filledPrefetchCount >= 128) {
      pc_entry->data.update_counter();
    }
#endif
  }

  uint64_t lookup_key = block_addr;
  latetpMetaTableEntry* metadata = metaTable->find(lookup_key);
  if (metadata) {
    metaTable->set_mru(lookup_key);
    if (!metadata->used) {
      metadata->used = true;
    }
  }

  // 2.issue: metadata table
  int cur_degree = 1;
#ifdef DYNAMIC_GLOBAL_DEGREE
  cur_degree = global_degree;
#else 
  cur_degree = pc_entry->data.degree;
#endif
#ifdef BW_DEGREE
  int cur_bw = get_dram_bw();
  if (cur_bw >= 12){
    cur_degree -= 2;
  }else if (cur_bw >= 8){
    cur_degree -= 1;
  }
#endif
  if (cur_degree > 0){
    issue_metatable(metaTable, lookup_key, cur_degree, pref_addr);
  }

  // 3.update
  // 3.1 update the metaTable
  if (last_addr != 0 && last_addr != block_addr) {
    uint64_t insert_key = last_addr;

    latetpMetaTableEntry* last_meta = metaTable->find(insert_key);

    if (last_meta) {
      bool matched = false;
      if (last_meta->correlated_addr == block_addr) {
        matched = true;
      } else {
        uint64_t victim_addr = last_meta->correlated_addr;
        latetpMetaTableEntry temp_entry(block_addr);
        metaTable->insert(insert_key, temp_entry);
      }
    } else {
      latetpMetaTableEntry temp_entry(block_addr);
      if (!metaTable->insert(insert_key, temp_entry)) {
        numEntriesinTable++;
      }
    }
  }

  // 3.2 update the pcTable
  bool already_exist = false;
  // for (auto& a : pc_entry->data.addrHistory) {
  //   if (a == block_addr) {
  //     already_exist = true;
  //     break;
  //   }
  // }
  if (!already_exist) 
  {
    pc_entry->data.addrHistory.push_front(block_addr);
    if (pc_entry->data.addrHistory.size() > pc_entry->data.lookahead + 1)
      pc_entry->data.addrHistory.pop_back();
  }

  for (int i = 0; i < pref_addr.size(); i++) {
#if FILTER_MODE == 1
    if (pf_filter.find(pref_addr[i]) != pf_filter.end())
      continue;
#elif FILTER_MODE == 2
    if (pf_filter.find(pref_addr[i]))
      continue;
#endif

    const bool success = prefetch_line({pref_addr[i] << LOG2_BLOCK_SIZE}, true, 0);
    if (success) {
      pc_entry->data.issuedPrefetchCount++;
#if FILTER_MODE == 1
      pf_filter.insert(pref_addr[i]);
#elif FILTER_MODE == 2
      pf_filter.add(pref_addr[i]);
#endif
    }
  }
  return metadata_in;
}

uint32_t latetp::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in,
                                       champsim::address ip)
{
  meta_table_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  if (prefetch) {
    auto pc_entry = pcTable->find(ip.to<uint64_t>());
    if (pc_entry) {
      pc_entry->data.filledPrefetchCount++;
    }
    unused_prefetches.insert(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (unused_prefetches.count(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)){
    useless_prefetch_num++;
  }

#if FILTER_MODE == 1
  pf_filter.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
#elif FILTER_MODE == 2
  pf_filter.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
#endif
  return metadata_in;
}

void latetp::prefetcher_final_stats()
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

void latetp::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
#ifdef ELABORATE_LOG
  logfile << std::dec << llc_cache->current_cycle() << " MSHRPFHIT " << std::hex << (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << " " << ip << std::dec
          << std::endl;
#endif
  auto pc_entry = pcTable->find(ip.to<uint64_t>());
  if (pc_entry) {
    pc_entry->data.latePrefetchCount++;
  }
  late_prefetch_num++;
  accurate_prefetch_num++;
}

void latetp::prefetcher_cycle_operate() {}

bool latetpMetaTable::insert(uint64_t key, const latetpMetaTableEntry& data)
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
