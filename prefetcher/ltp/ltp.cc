#include "ltp.h"

#include <cassert>
#include <utility>

int ltp::issue_metatable(ltpMetaTable* metaTable, uint64_t lookup, uint64_t pc, uint64_t degree, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < degree; i++) {
    ltpMetaTableEntry* candidate = metaTable->find(lookup);
    if (candidate == nullptr)
      break;
    addToUsedPool(lookup);
    if (candidate->correlatedAddr != 0) {
      if (!isAlreadyInQueue(addresses, candidate->correlatedAddr) && no_need_prefetch.find(candidate->correlatedAddr) == no_need_prefetch.end()) {
      // if (!isAlreadyInQueue(addresses, candidate->correlatedAddr) && !pf_filter.find(candidate->correlatedAddr)) {
      // if (!isAlreadyInQueue(addresses, candidate->correlatedAddr)) {
        champsim::address prefetch_addr{candidate->correlatedAddr << LOG2_BLOCK_SIZE};
        int pq_index = -1;
        const bool success = prefetch_line(prefetch_addr, true, 0, &pq_index);
        if (success) {
          no_need_prefetch.insert(candidate->correlatedAddr);
          pf_filter.add(candidate->correlatedAddr);
          addresses.push_back(candidate->correlatedAddr);
          issued++;
#ifdef ELABORATE_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) << " "
                  << (pcTable[pc].lookahead ? "LA" : "NLA") << " success " << std::dec << pq_index << " " << (pcTable[pc].degree) << std::endl;
#endif
        }else if (pq_index >= 0){
          // merge
#ifdef ELABORATE_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) << " "
                  << (pcTable[pc].lookahead ? "LA" : "NLA") << " merge " << std::dec << pq_index << " " << (pcTable[pc].degree) << std::endl;
#endif
        }else {
          // drop
#ifdef ELABORATE_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) << " "
                  << (pcTable[pc].lookahead ? "LA" : "NLA") << " drop " << std::dec << pq_index << " " << (pcTable[pc].degree) << std::endl;
#endif
          break;
        }
      }
      lookup = candidate->correlatedAddr;
    }
  }
  return issued;
}

int ltp::issue_mrbtable(ltpMRBTable* mrbTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t>& addresses)
{
  int issued = 0;
  for (int i = 0; i < globalDegree; i++) {
        ltpMetaTableEntry* candidate = metaTable->find(lookup);
    if (candidate == nullptr)
      break;
    addToUsedPool(lookup);
    if (candidate->correlatedAddr != 0) {
      if (!isAlreadyInQueue(addresses, candidate->correlatedAddr) && no_need_prefetch.find(candidate->correlatedAddr) == no_need_prefetch.end()) {
      // if (!isAlreadyInQueue(addresses, candidate->correlatedAddr) && !pf_filter.find(candidate->correlatedAddr)) {
      // if (!isAlreadyInQueue(addresses, candidate->correlatedAddr)) {
        champsim::address prefetch_addr{candidate->correlatedAddr << LOG2_BLOCK_SIZE};
        int pq_index = -1;
        const bool success = prefetch_line(prefetch_addr, true, 0, &pq_index);
        if (success) {
          no_need_prefetch.insert(candidate->correlatedAddr);
          pf_filter.add(candidate->correlatedAddr);
          addresses.push_back(candidate->correlatedAddr);
          issued++;
#ifdef ELABORATE_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MRB " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) << " "
                  << (pcTable[pc].lookahead ? "LA" : "NLA") << " success " << std::dec << pq_index << " " << (pcTable[pc].degree) << std::endl;
#endif
        }else if (pq_index >= 0){
          // merge
#ifdef ELABORATE_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MRB " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) << " "
                  << (pcTable[pc].lookahead ? "LA" : "NLA") << " merge " << std::dec << pq_index << " " << (pcTable[pc].degree) << std::endl;
#endif
        }else {
          // drop
#ifdef ELABORATE_LOG
          logfile << std::dec << llc_cache->current_cycle() << " ISSUE MRB " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) << " "
                  << (pcTable[pc].lookahead ? "LA" : "NLA") << " drop " << std::dec << pq_index << " " << (pcTable[pc].degree) << std::endl;
#endif
          break;
        }
      }
      lookup = candidate->correlatedAddr;
    }
  }
  return issued;
}

void ltp::outPrefetcherPGOInfo() {}

uint32_t ltp::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
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

  if (disablePF)
    return metadata_in;

  uint64_t pc = ip.to<uint64_t>();
  if (pc == 0) {
    return metadata_in;
  }

  vector<uint64_t> pref_addr;
  uint64_t block_addr = (addr.to<uint64_t>()) >> LOG2_BLOCK_SIZE;

  if (trainTable.find(pc) == trainTable.end()) {
    if (trainTable.size() < 128) {
      trainTable[pc] = TrainEntry();
    } else {
      std::map<uint64_t, TrainEntry>::iterator minPointer = trainTable.begin();
      for (std::map<uint64_t, TrainEntry>::iterator it = trainTable.begin(); it != trainTable.end(); ++it) {
        if (it->second.solved < minPointer->second.solved) {
          minPointer = it;
        }
      }
      if (!minPointer->second.protect) {
        trainTable.erase(minPointer);
        trainTable[pc] = TrainEntry();
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

  if (enableInsertFilter && enablePGO && profileInsertTable.find(pc) == profileInsertTable.end())
    return metadata_in;

  global_timestamp++;

  if (pcTable.find(pc) == pcTable.end()) {
    pcTable[pc] = PCTableEntry(block_addr, cache_hit);
  } else {
    PCTableEntry& pc_entry = pcTable[pc];
    uint64_t lastAddr = pc_entry.lookahead ? pc_entry.lastlastAddr : pc_entry.lastAddr;

    // 0.update pc_entry
    if (!cache_hit) {
      pc_entry.missCount += 1;
      pc_entry.coverageHistory <<= 1;
    } else if (useful_prefetch) {
      pc_entry.latePrefetchHistory <<= 1;
      pc_entry.coverageHistory = (pc_entry.coverageHistory << 1) | 1;
      pc_entry.accuratePrefetchCount += 1;
      if (pc_entry.accuratePrefetchCount >= 64 || pc_entry.issuedPrefetchCount >= 64) {
        pc_entry.shift_counter();
      }
    }

    // float accuracy = pc_entry.issuedPrefetchCount == 0 ? 0.0 : (1.0 * pc_entry.accuratePrefetchCount / pc_entry.issuedPrefetchCount);
    // float coverage = (pc_entry.usefulPrefetchCount + pc_entry.missCount) == 0 ? 0.0 : (1.0 * pc_entry.usefulPrefetchCount / (pc_entry.usefulPrefetchCount + pc_entry.missCount));
    // float coverage = 1.0 * __builtin_popcount(pc_entry.coverageHistory) / 64;
    // if (pc_entry.degree < MAX_DEGREE && pc_entry.latePrefetchHistory > 0 && accuracy > HIGH_ACCURACY_THRESHOLD) {
      // pc_entry.degree += 1;
    // }
    // if (pc_entry.degree > 1 && (accuracy < LOW_ACCURACY_THRESHOLD || coverage < LOW_COVERAGE_THRESHOLD)) {
      // pc_entry.degree -= 1;
    // }
    // if (accuracy < LOW_ACCURACY_THRESHOLD && coverage < LOW_COVERAGE_THRESHOLD) {
    // pc_entry.degree = 0;
    // }

    if (lastAddr != block_addr) {
      // 1.search: touch the metadata entry if exists, and then issue prefetches at step 2
      // MetaEntry *metadata = &(metaTable->find(block_addr)->data);
      ltpMetaTableEntry* metadata = metaTable->find(block_addr);
      ltpMRBTableEntry* reuseData = mrbTable->find(block_addr);
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
      int issued_by_metatable = issue_metatable(metaTable, lookup, pc, pc_entry.degree, pref_addr);
      pc_entry.issuedPrefetchCount += issued_by_metatable;
      if (enableMRB) {
        int issued_by_reuse = issue_mrbtable(mrbTable, lookup, pc, pref_addr);
        pc_entry.issuedPrefetchCount += issued_by_reuse;
      }
      for (auto& prefetch_address : pref_addr) {
        // llc_cache->prefetch_line(pc, addr, prefetch_address, FILL_L2, 0);
        prefetched_addr[prefetch_address >> LOG2_BLOCK_SIZE] = pc;
        trainTable[pc].issued += 1;
      }

      // 3.update
      // 3.1 update the metaTable
      if (lastAddr != 0) {
        ltpMetaTableEntry* lastMeta = metaTable->find(lastAddr);
        if (lastMeta) {
          bool matched = false;
          if (lastMeta->correlatedAddr == block_addr) {
            matched = true;
          }
          if (!matched) {
            uint64_t victimAddr = lastMeta->correlatedAddr;
            ltpMetaTableEntry temp_entry(block_addr);
            metaTable->insert(lastAddr, temp_entry, 1);

            // victim buffer logic
            if (profileReplTable[pc] > 1) {
              ltpMRBTableEntry* victimMeta = mrbTable->find(lastAddr);
              if (!victimMeta) {
                ltpMRBTableEntry temp_entry(victimAddr);
                mrbTable->insert(lastAddr, temp_entry);
              } else {
                if (victimMeta->correlatedAddr == victimAddr) {
                  if (victimMeta->counter < MRB_MAX_COUNTER) {
                    victimMeta->counter++;
                  }
                } else {
                  ltpMRBTableEntry temp_entry(victimAddr);
                  mrbTable->insert(lastAddr, temp_entry);
                }
              }
            }
          }
        } else {
          ltpMetaTableEntry temp_entry(block_addr);
          if (enablePGO && enablePGLRU && profileReplTable.find(pc) != profileReplTable.end())
            metaTable->insert(lastAddr, temp_entry, profileReplTable[pc]);
          else {
            if (!metaTable->insert(lastAddr, temp_entry, 1)) // lyq: profile中，prio = 0 ？
            {
              numEntriesinTable++;
            }
          }
          trainTable[pc].meta_inserted++;
        }
      }

      // 3.2 update the last addr
      pc_entry.lastlastAddr = pc_entry.lastAddr;
      pc_entry.lastAddr = block_addr;
    }
  }
  return metadata_in;
}

uint32_t ltp::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  no_need_prefetch.erase((evicted_addr.to<uint64_t>()) >> LOG2_BLOCK_SIZE);
  no_need_prefetch.insert((addr.to<uint64_t>()) >> LOG2_BLOCK_SIZE);

  pf_filter.erase((evicted_addr.to<uint64_t>()) >> LOG2_BLOCK_SIZE);
  return metadata_in;
}

void ltp::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
  uint64_t pc = ip.to<uint64_t>();
  if (pcTable.find(pc) != pcTable.end()) {
    PCTableEntry& pc_entry = pcTable[pc];
    pc_entry.latePrefetchCount += 1;
    pc_entry.latePrefetchHistory = (pc_entry.latePrefetchHistory << 1) | 1;
    pc_entry.accuratePrefetchCount += 1;

    if (!pc_entry.lookahead && pc_entry.latePrefetchCount >= 4){
      pc_entry.lookahead = true;
    }
    // if (!pc_entry.lookahead && (1.0 * __builtin_popcount(pc_entry.latePrefetchHistory) / 32) > LATE_THRESHOLD) {
    //   pc_entry.lookahead = true;
    // }
  }
}

void ltp::prefetcher_final_stats()
{
#ifdef ELABORATE_LOG
  logfile.close();
#endif
}

void ltp::prefetcher_cycle_operate() {}

/*
    If evict another valid entry: return true!
    else: return false!
*/
bool ltpMetaTable::insert(uint64_t key, const ltpMetaTableEntry& data, uint8_t priority)
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