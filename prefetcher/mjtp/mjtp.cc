#include "mjtp.h"

#include <cassert>
#include <utility>

void mjtp::invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t>& pref_addr, bool useful_prefetch)
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

  if (enableInsertFilter && !inTraining && profileInsertTable.find(ip) == profileInsertTable.end())
    return;

  global_timestamp++;

  // 1.search
  // MetaEntry *metadata = &(metaTable->find(block_addr)->data);
  mjtpMetaTableEntry* metadata = metaTable->find(block_addr);
  mjtpMRBTableEntry* reuseData = mrbTable->find(block_addr);

  uint64_t lastAddr = pcTable.find(ip) != pcTable.end() ? pcTable[ip] : 0;
  if (lastAddr == block_addr)
    return;
  if (reuseData) {
    if (reuseData->counter < MRB_MAX_COUNTER)
      reuseData->counter++;
    mrbTable->set_mru(block_addr);
  }
  if (metadata) {
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
    mjtpMetaTableEntry* lastMeta = metaTable->find(lastAddr);
    if (lastMeta) {
      bool matched = false;
      if (lastMeta->correlatedAddr == block_addr) {
        matched = true;
      }
      if (!matched) {
        uint64_t victimAddr = lastMeta->correlatedAddr;
        mjtpMetaTableEntry temp_entry(block_addr);
        metaTable->insert(lastAddr, temp_entry, ip, useful_prefetch);

        // victim buffer logic
        if (profileReplTable[ip] > 1) {
          mjtpMRBTableEntry* victimMeta = mrbTable->find(lastAddr);
          if (!victimMeta) {
            mjtpMRBTableEntry temp_entry(victimAddr);
            mrbTable->insert(lastAddr, temp_entry);
          } else {
            if (victimMeta->correlatedAddr == victimAddr) {
              if (victimMeta->counter < MRB_MAX_COUNTER) {
                victimMeta->counter++;
              }
            } else {
              mjtpMRBTableEntry temp_entry(victimAddr);
              mrbTable->insert(lastAddr, temp_entry);
            }
          }
        }
      }
    } else {
      mjtpMetaTableEntry temp_entry(block_addr);
      if (!inTraining && enablePGLRU && profileReplTable.find(ip) != profileReplTable.end())
        metaTable->insert(lastAddr, temp_entry, ip, useful_prefetch);
      else {
        if (!metaTable->insert(lastAddr, temp_entry, ip, useful_prefetch)) // lyq: profile中，prio = 0 ？
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

int mjtp::issue_metatable(mjtpMetaTable* metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {

    mjtpMetaTableEntry* candidate = metaTable->find(lookup);

    stats_md_lookups++;
    if (candidate != nullptr) {
      stats_md_hits++;
    }
    if (candidate == nullptr)
      break;
    addToUsedPool(lookup);
    if (candidate->correlatedAddr != 0) {

      if (!isAlreadyInQueue(addresses, candidate->correlatedAddr << LOG2_BLOCK_SIZE)) {
        addresses.push_back(candidate->correlatedAddr << LOG2_BLOCK_SIZE);

#ifdef ELABORATE_LOG
        logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr)
                << std::endl;
#endif
        issued++;
#ifdef MJ_LYQREP
        (metaTable->recent_prefetches)[candidate->correlatedAddr] = lookup;
#endif
      }
      lookup = candidate->correlatedAddr;
    }
  }
  return issued;
}

int mjtp::issue_mrbtable(mjtpMRBTable* mrbTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses)
{
  std::cout << "deprecated,Should not issue" << std::endl;
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
    mjtpMRBTableEntry* candidate = mrbTable->find(lookup);
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

void mjtp::outPrefetcherPGOInfo() {}

uint32_t mjtp::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
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
  vector<uint64_t> prefetch_candidate;

  invoke_prefetcher(ip.to<uint64_t>(), addr.to<uint64_t>(), cache_hit, 0, prefetch_candidate, useful_prefetch);

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

uint32_t mjtp::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void mjtp::prefetcher_final_stats()
{
#ifdef ELABORATE_LOG
  logfile.close();
#endif
  std::cout << "mt_lookups: " << stats_md_lookups << std::endl;
  std::cout << "mt_hits: " << stats_md_hits << std::endl;
}

void mjtp::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
#ifdef ELABORATE_LOG
  logfile << std::dec << llc_cache->current_cycle() << " MSHRPFHIT " << std::hex << (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) << " " << ip << std::dec
          << std::endl;
#endif
}

void mjtp::prefetcher_cycle_operate() {}

bool mjtpMetaTable::insert(uint64_t key, const mjtpMetaTableEntry& data, uint64_t pc, bool useful_prefetch)
{
  reverse_metatable[data.correlatedAddr].insert(key);
  Entry victim_entry = Super::insert(key, data); // SetAssociativeCache<mjtpMetaTableEntry>::insert(key, data);
  // Super::touch(key);

  uint64_t set = key % this->num_sets;
  uint64_t tag = key / this->num_sets;
  int way = this->cams[set][tag];
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

#ifdef MJ_TRIGGER
  this->mj_update(key, set, way, victim_entry.key == key, pc);
#else
#ifdef MJ_TARGET
  this->mj_update(data.correlatedAddr, set, way, useful_prefetch, pc);
#else
#ifdef MJ_TRIGGER_TARGET
  // uint64_t mj_key = (key << 32) | data.correlatedAddr & ((1 << 32) - 1);
  uint64_t mj_key = CRC_HASH(key ^ data.correlatedAddr);
  this->mj_update(mj_key, set, way, victim_entry.key == key && victim_entry.data.correlatedAddr == data.correlatedAddr, pc);
#else
#ifdef MJ_LYQREP
  uint64_t mj_key = CRC_HASH(key ^ data.correlatedAddr);
  this->mj_update(mj_key, set,  way, victim_entry.key == key && victim_entry.data.correlatedAddr == data.correlatedAddr, pc);

  if (recent_prefetches.find(data.correlatedAddr) != recent_prefetches.end()){
    uint64_t trigger = recent_prefetches[data.correlatedAddr];
    if (trigger != key){
      Entry victim = Super::insert(trigger, data);
      reverse_metatable[data.correlatedAddr].insert(trigger);
      if (victim.valid){
        reverse_metatable[victim.data.correlatedAddr].erase(victim.key);
      }
      set = trigger % this->num_sets;
      tag = trigger / this->num_sets;
      way = this->cams[set][tag];
      mj_key = CRC_HASH(trigger ^ data.correlatedAddr);
      this->mj_update(mj_key, set, way, true, pc);
      // only_update_sample(mj_key, set, way, true, pc);
      // only_set_repl(mj_key, set, way, true, pc);
    }
    recent_prefetches.erase(data.correlatedAddr);
  }
#endif
#endif
#endif
#endif

#ifdef ELABORATE_LOG
  pp->logfile << std::dec << pp->llc_cache->current_cycle() << " ADD " << std::hex << key << " " << data.correlatedAddr << std::endl;
#endif
  return ret;
}
