#include "ptp.h"

#include <cassert>
#include <utility>

void ptp::invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t>& pref_addr)
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
    if (cache_hit && prefetched_addr.find(block_addr) != prefetched_addr.end()) {
      trainTable[prefetched_addr[block_addr]].solved += 1;
      prefetched_addr.erase(block_addr);
    }
  }

  if (enableInsertFilter && enablePGO && profileInsertTable.find(ip) == profileInsertTable.end())
    return;

  global_timestamp++;

  // 1.search
  // MetaEntry *metadata = &(metaTable->find(block_addr)->data);
  ptpMetaTableEntry* metadata = metaTable->find(block_addr);
  ptpMRBTableEntry* reuseData = mrbTable->find(block_addr);

  uint64_t lastAddr = pcTable.find(ip) != pcTable.end() ? pcTable[ip] : 0;
  if (lastAddr != block_addr) {
    if (reuseData) {
      if (reuseData->counter < MRB_MAX_COUNTER)
        reuseData->counter++;
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
      ptpMetaTableEntry* lastMeta = metaTable->find(lastAddr);
      if (lastMeta) {
        bool matched = false;
        if (lastMeta->correlatedAddr == block_addr) {
          matched = true;
        }
        if (!matched) {
          uint64_t victimAddr = lastMeta->correlatedAddr;
          ptpMetaTableEntry temp_entry(block_addr);
          metaTable->insert(lastAddr, temp_entry, 1);

          // victim buffer logic
          if (profileReplTable[ip] > 1) {
            ptpMRBTableEntry* victimMeta = mrbTable->find(lastAddr);
            if (!victimMeta) {
              ptpMRBTableEntry temp_entry(victimAddr);
              mrbTable->insert(lastAddr, temp_entry);
            } else {
              if (victimMeta->correlatedAddr == victimAddr) {
                if (victimMeta->counter < MRB_MAX_COUNTER) {
                  victimMeta->counter++;
                }
              } else {
                ptpMRBTableEntry temp_entry(victimAddr);
                mrbTable->insert(lastAddr, temp_entry);
              }
            }
          }
        }
      } else {
        ptpMetaTableEntry temp_entry(block_addr);
        if (enablePGO && enablePGLRU){ 
          if (profileReplTable.find(ip) != profileReplTable.end())
            metaTable->insert(lastAddr, temp_entry, profileReplTable[ip]);
        }
        else {
          if (!metaTable->insert(lastAddr, temp_entry, 1)) // lyq: profile中，prio = 0 ？
          {
            numEntriesinTable++;
          }
        }
        trainTable[ip].meta_inserted++;
      }
    }

    // 3.2 update the pcTable
    pcTable[ip] = block_addr;
  }

  if (lastAddr == block_addr) {
    if (!cache_hit && GPQ.size() > 0) {
      uint64_t triggerIP = 0;
      triggerIP = GPQ.rbegin()->ip;
      
      if (triggerIP) {
        triggerIP = hash_xor(triggerIP);
#if PC_META_TABLE_MODE == 0
        GPMetaTable[triggerIP] = {block_addr, ++GPMtime};
#elif PC_META_TABLE_MODE == 1
        GPMetaTable->insert(triggerIP, {block_addr, ++GPMtime});
        GPMetaTable->set_mru(triggerIP);
#elif PC_META_TABLE_MODE == 2
        auto victim = GPMetaTable->insert(triggerIP, {block_addr, ++GPMtime});
        if (!victim.valid || victim.key != triggerIP) 
          GPMetaTable->set_default(triggerIP);
        else if (victim.valid && victim.key == triggerIP && victim.data.block_addr == block_addr)
          GPMetaTable->touch(triggerIP);
#endif
      }
    }
  }

  uint64_t lookupPC = hash_xor(ip);
#if PC_META_TABLE_MODE == 0
  if (GPMetaTable.find(lookupPC) != GPMetaTable.end()) {
    pref_addr.push_back(GPMetaTable[lookupPC].block_addr << LOG2_BLOCK_SIZE);
    GPM_issued_prefetches.insert(GPMetaTable[lookupPC].block_addr);
    // GPMlogfile << GPMtime - GPMetaTable[lookupPC].last_touch_time << endl;
    GPMetaTable[lookupPC].last_touch_time = GPMtime;
  }
#elif PC_META_TABLE_MODE == 1
  auto gpm_entry = GPMetaTable->find(lookupPC);
  if (gpm_entry) {
    GPMetaTable->set_mru(lookupPC);
    pref_addr.push_back(gpm_entry->data.block_addr << LOG2_BLOCK_SIZE);
    GPM_issued_prefetches.insert(gpm_entry->data.block_addr);
    gpm_entry->data.last_touch_time = GPMtime;
  }
#elif PC_META_TABLE_MODE == 2
  auto gpm_entry = GPMetaTable->find(lookupPC);
  if (gpm_entry) {
    GPMetaTable->touch(lookupPC);
    pref_addr.push_back(gpm_entry->data.block_addr << LOG2_BLOCK_SIZE);
    GPM_issued_prefetches.insert(gpm_entry->data.block_addr);
    gpm_entry->data.last_touch_time = GPMtime;
  }
#endif

#if ONLY_TRIGGER_ON_MISS
  if (!cache_hit) {
#else
  if (true) {
#endif
    bool already_in_queue = false;
    for (const auto& entry : GPQ) {
      if (entry.ip == ip) {
        already_in_queue = true;
        break;
      }
    }
    if (!already_in_queue) {
      if (GPQ.size() < GPQ_SIZE) {
        GPQ.push_front({ip, llc_cache->current_cycle()});
      } else {
        GPQ.pop_back();
        GPQ.push_front({ip, llc_cache->current_cycle()});
      }
    }
  }
}

int ptp::issue_metatable(ptpMetaTable* metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    ptpMetaTableEntry* candidate = metaTable->find(lookup);
    if (candidate == nullptr)
      break;
    addToUsedPool(lookup);
    if (candidate->correlatedAddr != 0) {

      if (!isAlreadyInQueue(addresses, candidate->correlatedAddr << LOG2_BLOCK_SIZE)) {
#ifdef ELABORATE_LOG
        logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr)
                << std::endl;
#endif
        addresses.push_back(candidate->correlatedAddr << LOG2_BLOCK_SIZE);
        issued++;
      }
      lookup = candidate->correlatedAddr;
    }
  }
  return issued;
}

int ptp::issue_mrbtable(ptpMRBTable* mrbTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    ptpMRBTableEntry* candidate = mrbTable->find(lookup);
    if (candidate == nullptr)
      break;
    addToUsedPool(lookup);
    if (candidate->correlatedAddr != 0) {
      lookup = candidate->correlatedAddr;
      if (!isAlreadyInQueue(addresses, candidate->correlatedAddr << LOG2_BLOCK_SIZE)) {
#ifdef ELABORATE_LOG
        logfile << std::dec << llc_cache->current_cycle() << " ISSUE MRB " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr)
                << std::endl;
#endif
        addresses.push_back(candidate->correlatedAddr << LOG2_BLOCK_SIZE);
        issued++;
      }
    }
  }
  return issued;
}

void ptp::outPrefetcherPGOInfo() {}

uint32_t ptp::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
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

  if (GPM_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) && latepf != "NO") {
    GPM_late_prefetches++;
    GPM_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (cache_hit && GPM_filled_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    GPM_useful_prefetches++;
    GPM_filled_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
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

uint32_t ptp::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  if (prefetch && GPM_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    GPM_filled_prefetches.insert(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    GPM_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (GPM_filled_prefetches.count(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    GPM_useless_prefetches++;
    GPM_filled_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  return metadata_in;
}

void ptp::prefetcher_final_stats()
{
#ifdef ELABORATE_LOG
  logfile.close();
#endif

#if PC_META_TABLE_MODE == 0
  cout << "PC_table_size " << pcTable.size() << endl;
  cout << "GPM_table_size " << GPMetaTable.size() << endl;
#endif
  cout << "GPM_late_prefetches " << GPM_late_prefetches << endl;  
  cout << "GPM_useful_prefetches " << GPM_useful_prefetches << endl;
  cout << "GPM_useless_prefetches " << GPM_useless_prefetches << endl;
  uint64_t GPM_accurate_prefetches = GPM_late_prefetches + GPM_useful_prefetches;
  uint64_t GPM_total_prefetches = GPM_late_prefetches + GPM_useful_prefetches + GPM_useless_prefetches;
  cout << "GPM_accuracy " << (GPM_total_prefetches ? (double)GPM_accurate_prefetches / GPM_total_prefetches : 0) << endl;
  cout << "GPM_laterate " << (GPM_accurate_prefetches ? (double)GPM_late_prefetches / GPM_accurate_prefetches : 0) << endl;
}

void ptp::prefetcher_cycle_operate() {}

/*
    If evict another valid entry: return true!
    else: return false!
*/
bool ptpMetaTable::insert(uint64_t key, const ptpMetaTableEntry& data, uint8_t priority)
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
#ifdef ELABORATE_LOG
    std::string reason;
    if (victim_entry.tag != tag) {
      reason = "CAPACITY";
    } else {
      reason = "CONFLICT";
    }
    pp->logfile << std::dec << pp->llc_cache->current_cycle() << " EVICT " << reason << " " << std::hex << victim_entry.key << " "
                << victim_entry.data.correlatedAddr << std::endl;
#endif
    ret = true;
  }
#ifdef ELABORATE_LOG
  pp->logfile << std::dec << pp->llc_cache->current_cycle() << " ADD " << std::hex << key << " " << data.correlatedAddr << std::endl;
#endif
  return ret;
}