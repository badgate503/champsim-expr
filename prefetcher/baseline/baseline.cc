#include "baseline.h"

#include <cassert>
#include <utility>

int baseline::issue_metatable(baselineMetaTable* metaTable, uint64_t pc, uint64_t lookup, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    baselineMetaTableEntry* candidate = metaTable->find(lookup);
    /* Energy */
    markov_read++;
    MT_lookups++;
    if (candidate == nullptr)
      break;
    MT_hits++;
    if (candidate->correlated_addr != 0) {
      if (!isAlreadyInQueue(addresses, candidate->correlated_addr)) {
        addresses.push_back(candidate->correlated_addr);
        meta_table_issued_prefetches++;
        meta_table_prefetches.insert(candidate->correlated_addr);
        issued++;
        int pq_index = -1;
        champsim::address prefetch_addr{(candidate->correlated_addr) << LOG2_BLOCK_SIZE};
        const bool success = prefetch_line(prefetch_addr, true, 0, &pq_index);
        if (success) {
#ifdef MISS_CLASS_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlated_addr)
                  << " success " << std::dec << pq_index << " " << std::endl;
#endif
        } else if (pq_index >= 0) {
#ifdef MISS_CLASS_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlated_addr)
                  << " merge " << std::dec << pq_index << " " << std::endl;
#endif
        } else {
#ifdef MISS_CLASS_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlated_addr)
                  << " drop " << std::dec << pq_index << " " << std::endl;
#endif
        }
      }
      lookup = candidate->correlated_addr;
    } 
  }
  return issued;
}

uint32_t baseline::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
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
      std::string type_str = "";
      if(type == access_type::PREFETCH) {
        type_str = "PREFETCH";
      } else if(type == access_type::LOAD) {
        type_str = "LOAD";
      } else if(type == access_type::RFO) {
        type_str = "RFO";
      } else if(type == access_type::WRITE) {
        type_str = "WRITE";
      } else if(type == access_type::TRANSLATION) {
        type_str = "TRANSLATION";
      }
      logfile << std::dec << llc_cache->current_cycle() << " MISS " << latepf << " " << std::hex << pf_addr << " " << ip << " " << type_str << " "<< last_addr;
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

  if(!warmup_reset && !llc_cache->warmup){
    reset_stat_counters();
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
  /* Energy */
  training_unit_read++;
  if (pc_entry) {
    last_addr = pc_entry->data.front(); 
    /* Energy */
    training_unit_write++;
  } else {
    std::deque<uint64_t> temp(1, 0);
    pcTable->insert(pc, temp);

    pc_entry = pcTable->find(pc);
    /* Energy */
    training_unit_write++;
  }
  pcTable->set_mru(pc);

  uint64_t lookup_key = block_addr;

  baselineMetaTableEntry* metadata = metaTable->find(lookup_key);
  if (metadata) {
    metaTable->set_mru(lookup_key);
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

    baselineMetaTableEntry* last_meta = metaTable->find(insert_key);

    if (last_meta) {
      bool matched = false;
      if (last_meta->correlated_addr == block_addr) {
        matched = true;
      } else {
        uint64_t victim_addr = last_meta->correlated_addr;
        baselineMetaTableEntry temp_entry(block_addr);
        metaTable->insert(insert_key, temp_entry);
        /* Energy */
        markov_write++;
        MT_inserts++;
      }
    } else {
      baselineMetaTableEntry temp_entry(block_addr);
      if (!metaTable->insert(insert_key, temp_entry)) {
        numEntriesinTable++;
      }
      /* Energy */
      markov_write++;
      MT_inserts++;
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

  return metadata_in;
}

uint32_t baseline::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  meta_table_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
#ifdef MISS_CLASS_LOG
  logfile << std::dec << llc_cache->current_cycle() << " CACHEFILL " << std::hex << (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << " " << (evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << (prefetch ? " PREFETCH" : " NOPREFETCH") << std::dec
          << std::endl;
#endif
  return metadata_in;
}

void baseline::prefetcher_final_stats()
{
#ifdef MISS_CLASS_LOG
  logfile.close();
#endif
  cout << "MT_lookups " << MT_lookups << endl;
  cout << "MT_hits " << MT_hits << endl;
  cout << "MT_inserts " << MT_inserts << endl;

  cout << "MT_hitrate " << (MT_lookups ? (double)MT_hits / MT_lookups : 0) << endl;
  cout << "MT_issuedpf " << meta_table_issued_prefetches << endl;
  cout << "MT_accuratepf " << meta_table_accurate_prefetches << endl;
  cout << "MT_accuracy " << (meta_table_issued_prefetches ? (double)meta_table_accurate_prefetches / meta_table_issued_prefetches : 0) << endl;

  cout << "training_unit_read " << training_unit_read << endl;
  cout << "training_unit_write " << training_unit_write << endl;
  cout << "4_way_markov_table_read " << markov_read << endl;
  cout << "4_way_markov_table_write " << markov_write << endl;
}

void baseline::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
#ifdef MISS_CLASS_LOG
  logfile << std::dec << llc_cache->current_cycle() << " MSHRPFHIT " << std::hex << (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << " " << ip << std::dec
          << std::endl;
#endif
}

void baseline::prefetcher_cycle_operate() {}

bool baselineMetaTable::insert(uint64_t key, const baselineMetaTableEntry& data)
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
