#include "prophet.h"

#include <cassert>
#include <utility>

void prophet::invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t>& pref_addr)
{
  if (disablePF)
    return;
  if (ip == 0) {
    return;
  }

  uint64_t block_addr = addr >> LOG2_BLOCK_SIZE;

  if (trainTable.find(ip) == trainTable.end()) { 

    if (trainTable.size() < 128) {
      trainTable[ip] = TrainEntry();
    } else {
      std::map<uint64_t, TrainEntry>::iterator minPointer = trainTable.begin();
      for (std::map<uint64_t, TrainEntry>::iterator it = trainTable.begin(); it != trainTable.end(); ++it) {
        if (it->second.solved < minPointer->second.solved) {
          minPointer = it;
        }
      }
      if (!minPointer->second.protect) {
        trainTable.erase(minPointer);
        trainTable[ip] = TrainEntry();
      } else {
        minPointer->second.protect = false;
      }
    }
  } else {
    /* Energy */
    tu_read_n++;
    if (cache_hit && prefetched_addr.find(block_addr) != prefetched_addr.end()) {
      trainTable[prefetched_addr[block_addr]].solved += 1;
      prefetched_addr.erase(block_addr);
    }
  }

  if (enableInsertFilter && !inTraining && profileInsertTable.find(ip) == profileInsertTable.end())
    return;

  global_timestamp++;

  // 1.search
  ProphetMetaTableEntry* metadata = metaTable->find(block_addr);
  /* Energy */
  md_read_n++;
  ProphetMRBTableEntry* reuseData = mrbTable->find(block_addr);
  /* Energy */
  rb_read_n++;

  uint64_t lastAddr = 0;
  auto pc_entry = pcTable->find(ip);
  /* Energy */
  tu_read_n++;
  if (pc_entry) {
    /* Energy */
    tu_write_n++;
    lastAddr = pc_entry->data;
    
  } else {
    pcTable->insert(ip, 0);
    /* Energy */
    tu_write_n++;
    pc_entry = pcTable->find(ip);
  }
  pcTable->set_mru(ip);

  if (lastAddr == block_addr)
    return;
  if (reuseData) {
    if (reuseData->counter < MRB_MAX_COUNTER) {
      reuseData->counter++;
      /* Energy */
      rb_write_n++;
    }
    mrbTable->set_mru(block_addr);
  }
  if (metadata) {
    metaTable->set_mru(block_addr);
    if (profileUtiTable.find(block_addr) != profileUtiTable.end()) {
      profileUtiTable[block_addr]++;
    } else {
      profileUtiTable[block_addr] = 0;
    }
    if (!metadata->used) {
      metadata->used = true;
      /* Energy */
      md_write_n++;
      // trainTable[metadata->pc].meta_used++;
    }
  }

  // 2.issue: metadata table
  uint64_t lookup = block_addr;
  int issued_by_metatable = issue_metatable(metaTable, lookup, ip, pref_addr);
  if (enableMRB) {
    int issued_by_reuse = issue_mrbtable(mrbTable, lookup, ip, pref_addr);
  }
  for (auto& prefetch_address : pref_addr) {
    // llc_cache->prefetch_line(ip, addr, prefetch_address, FILL_L2, 0);
    prefetched_addr[prefetch_address >> LOG2_BLOCK_SIZE] = ip;
    trainTable[ip].issued += 1;
  }

  // 3.update
  // 3.1 update the metaTable

  if (lastAddr != 0) {
    ProphetMetaTableEntry* lastMeta = metaTable->find(lastAddr);
    /* Energy */
    md_read_n++;
    if (lastMeta) {
      bool matched = false;
      if (lastMeta->correlatedAddr == block_addr) {
        matched = true;
      }
      if (!matched) {
        uint64_t victimAddr = lastMeta->correlatedAddr;
        ProphetMetaTableEntry temp_entry(block_addr);
        metaTable->insert(lastAddr, temp_entry, profileReplTable[ip]);
        /* Energy */
        md_write_n++;
        MT_inserts++;

        // victim buffer logic
        if (profileReplTable[ip] > 1) {
          ProphetMRBTableEntry* victimMeta = mrbTable->find(lastAddr);
          /* Energy */
          rb_read_n++;
          /* Energy */
          rb_write_n++;
          if (!victimMeta) {
            ProphetMRBTableEntry temp_entry(victimAddr);
            mrbTable->insert(lastAddr, temp_entry);
            
          } else {
            if (victimMeta->correlatedAddr == victimAddr) {
              if (victimMeta->counter < MRB_MAX_COUNTER) {
                victimMeta->counter++;
              }
            } else {
              ProphetMRBTableEntry temp_entry(victimAddr);
              mrbTable->insert(lastAddr, temp_entry);
            }
          }
        }
      }
    } else {
      ProphetMetaTableEntry temp_entry(block_addr);
      if (!inTraining && enablePGLRU) {
        if (profileReplTable.find(ip) != profileReplTable.end()){
          metaTable->insert(lastAddr, temp_entry, profileReplTable[ip]);
          /* Energy */
          md_write_n++;
          MT_inserts++;
        }
      } else {
        MT_inserts++;
        /* Energy */
        rb_write_n++;
        if (!metaTable->insert(lastAddr, temp_entry, 1)) // lyq: when profiling，prio = 1 ？
        {
          numEntriesinTable++;
        }
      }
      trainTable[ip].meta_inserted++;
    }
  }

  // 3.2 update the pcTable
  pc_entry->data = block_addr;
}

int prophet::issue_metatable(ProphetMetaTable* metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  MT_lookup_reqs++;
  bool find_success = false;
  for (int i = 0; i < globalDegree; i++) {
    ProphetMetaTableEntry* candidate = metaTable->find(lookup);
    /* Energy */
    md_read_n++;
    MT_lookups++;
    if (candidate == nullptr)
      break;
    addToUsedPool(lookup);
    if (candidate->correlatedAddr != 0) {
      MT_hits++;
      find_success = true;
      if (!isAlreadyInQueue(addresses, candidate->correlatedAddr << LOG2_BLOCK_SIZE)) {
        addresses.push_back(candidate->correlatedAddr << LOG2_BLOCK_SIZE);
        issued++;
      }
      lookup = candidate->correlatedAddr;
    }
  }
  if (find_success){
    MT_lookup_returns++;
  }
  return issued;
}

int prophet::issue_mrbtable(ProphetMRBTable* mrbTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    ProphetMRBTableEntry* candidate = mrbTable->find(lookup);
    /* Energy */
    rb_read_n++;
    if (candidate == nullptr)
      break;
    addToUsedPool(lookup);
    if (candidate->correlatedAddr != 0) {
      lookup = candidate->correlatedAddr;
      if (!isAlreadyInQueue(addresses, candidate->correlatedAddr << LOG2_BLOCK_SIZE)) {
        addresses.push_back(candidate->correlatedAddr << LOG2_BLOCK_SIZE);
        issued++;
      }
    }
  }
  return issued;
}

void prophet::outPrefetcherPGOInfo() {}

uint32_t prophet::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                           uint32_t metadata_in, std::string latepf)
{
  if (!warmup_reset && !llc_cache->warmup) {
    reset_stat_counters();
  }
  vector<uint64_t> prefetch_candidate;

  invoke_prefetcher(ip.to<uint64_t>(), addr.to<uint64_t>(), cache_hit, 0, prefetch_candidate);

  if (prefetch_candidate.size() == 0)
    return metadata_in;

  for (int i = 0; i < prefetch_candidate.size(); i++) {
    uint64_t p_addr = prefetch_candidate[i];

    if (p_addr == 0)
      break;
    champsim::address prefetch_addr{p_addr};
    const bool success = prefetch_line(prefetch_addr, true, 0);
  }
  return metadata_in;
}

uint32_t prophet::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void prophet::prefetcher_final_stats()
{
  cout << "MT_lookups " << MT_lookups << endl;
  cout << "MT_hits " << MT_hits << endl;
  cout << "MT_lookup_reqs " << MT_lookup_reqs << endl;
  cout << "MT_lookup_returns " << MT_lookup_returns << endl;
  cout << "MT_inserts " << MT_inserts << endl;

  if (inTraining) {
    
    hint_file << numEntriesinTable << std::endl;

    for (std::map<uint64_t, TrainEntry>::iterator it = trainTable.begin(); it != trainTable.end(); ++it) {
      // PC, solved, issued, accuracy
      uint64_t pc = it->first;
      float accuracy = 0;
      if (it->second.issued)
        accuracy = 1.0 * it->second.solved / it->second.issued;
      int priority = 0;
      if (accuracy <= 0.15) {
        priority = 0;
      } else if (accuracy < 0.25) {
        priority = 1;
      } else if (accuracy < 0.50) {
        priority = 2;
      } else if (accuracy < 0.75) {
        priority = 3;
      } else {
        priority = 4;
      }
      if (priority != 0)
        hint_file << std::hex << pc << std::dec << "," << priority << endl; // "," << accuracy;
                                                                    // f << "," << it->second.solved << "," << it->second.issued << endl;
    }
    hint_file.close();
  }

  cout << "multipath_victim_buffer_read " << rb_read_n << endl;
  cout << "multipath_victim_buffer_write " << rb_write_n << endl;
  cout << (16 - waysForCache) << "_way_markov_table_read " << md_read_n << endl;
  cout << (16 - waysForCache) << "_way_markov_table_write " << md_write_n << endl;
  cout << "training_unit_read " << tu_read_n << endl;
  cout << "training_unit_write " << tu_write_n << endl;

}

void prophet::prefetcher_cycle_operate() {}

bool ProphetMetaTable::insert(uint64_t key, const ProphetMetaTableEntry& data, uint8_t priority)
{
  reverse_metatable[data.correlatedAddr].insert(key);

  Entry victim_entry = Super::insert(key, data);
  Super::set_mru(key);
  uint64_t index = key % this->num_sets;
  uint64_t tag = key / this->num_sets;
  int way = this->cams[index][tag];
  priority_pgo[index][way] = priority;
  bool ret = false;
  if (victim_entry.valid) {
    reverse_metatable[victim_entry.data.correlatedAddr].erase(victim_entry.key);
    ret = true;
  }
  return ret;
}
