#include "prism.h"

int prism::issue_mainMetatable(uint64_t block_addr, int degree)
{
  int issued = 0;
  uint64_t lookup = block_addr;
  for (int i = 0; i < degree; i++) {
    auto entry = mainMetaTable->find(lookup);
    main_table_lookups++;
    if (entry == nullptr || entry->target_addr == 0)
      break;
    main_table_hits++;
#ifdef PREFETCH_FILTER
    if (pf_filter->find(entry->target_addr)) {
      lookup = entry->target_addr;
      continue;
    }
#endif

    const bool success = prefetch_line({entry->target_addr << LOG2_BLOCK_SIZE}, true, 0);
    if (success) {
      issued++;
      main_table_issued_prefetches++;
      main_table_prefetches.insert(entry->target_addr);
#ifdef INIT_RESIZE
      if (!resized && !llc_cache->warmup) {
        resize_issued_prefetch++;
      }
#endif
#ifdef PREFETCH_FILTER
      pf_filter->insert(entry->target_addr);
#endif
#ifdef ELABORATE_LOG
      logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) << std::endl;
#endif
    } else {
      break;
    }
    lookup = entry->target_addr;
  }
  return issued;
}

uint32_t prism::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
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

  if (pc == 0 || block_addr == 0)
    return metadata_in;

  if (!llc_cache->warmup && (type == access_type::LOAD || type == access_type::RFO)) {
    demand++;
  }
#ifdef INIT_RESIZE
  if (!llc_cache->warmup && useful_prefetch) {
    resize_useful_prefetch++;
  }
#endif

  if (!resized && !llc_cache->warmup && demand >= INIT_RESIZE_WINDOW) {
#ifdef INIT_RESIZE
    update_main_metatable_size();
#endif
#ifdef PC_TRIGGER_PREFETCHING
    update_pc_metatable_size();
#endif
    resize_cache();
    resized = true;
  }

  // stat
  if (main_table_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    main_table_accurate_prefetches++;
    main_table_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
#ifdef PC_TRIGGER_PREFETCHING
  if (PCM_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE) && latepf != "NO") {
    PCM_late_prefetches++;
    PCM_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (cache_hit && PCM_filled_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_useful_prefetches++;
    PCM_filled_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
#endif

#ifdef CONFLICT_PREFETCHING
  if (conflict_table_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    conflict_table_accurate_prefetches++;
    conflict_table_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
#endif

  auto pc_entry = pcTable->find(pc);
  bool train_pc_meta_table = false;
  if (pc_entry) {
    pcTable->set_mru(pc);
    uint64_t last_addr = pc_entry->data.addrHistory.front();
    if (last_addr != block_addr) {
      // prefetching
#ifdef CONFLICT_PREFETCHING
      uint64_t conflictTableKey = last_addr ^ (block_addr << 5) ^ (block_addr >> 7);
      conflictTableKey ^= conflictTableKey >> 16;

      conflict_table_lookups++;
      auto conflict_meta_entry = conflictMetaTable->find(conflictTableKey);
      if (conflict_meta_entry) {
        conflict_table_hits++;
        conflictMetaTable->touch(conflictTableKey);
#ifdef PREFETCH_FILTER
        if (!pf_filter->find(conflict_meta_entry->data.target_addr))
#endif
        {
          prefetch_line({conflict_meta_entry->data.target_addr << LOG2_BLOCK_SIZE}, true, 0);
#ifdef PREFETCH_FILTER
          pf_filter->insert(conflict_meta_entry->data.target_addr);
#endif
          conflict_table_issued_prefetches++;
          conflict_table_prefetches.insert(conflict_meta_entry->data.target_addr);
        }
      }
#endif

      issue_mainMetatable(block_addr, pc_entry->data.degree);
#ifdef TOUCH_FOR_TRIGGER
      if (mainMetaTable->find(block_addr))
        mainMetaTable->touch(block_addr, pc);
#endif

      // training
      // auto last_metadata = mainMetaTable->find(last_addr);
      uint64_t trigger_addr = 0;
      if (pc_entry->data.addrHistory.size() > pc_entry->data.lookahead)
        trigger_addr = pc_entry->data.addrHistory[pc_entry->data.lookahead];
      else
        trigger_addr = pc_entry->data.addrHistory.back();

      if (trigger_addr != 0) {
        auto exist_metadata = mainMetaTable->find(trigger_addr);
        if (exist_metadata) {
          if (exist_metadata->target_addr == block_addr) {
#ifndef TOUCH_FOR_TRIGGER
            mainMetaTable->touch(trigger_addr, pc);
#endif
          } else {
            // metadata conflict
#ifdef CONFLICT_PREFETCHING
            uint64_t addr_before_trigger = 0;
            if (pc_entry->data.addrHistory.size() > pc_entry->data.lookahead + 1)
              addr_before_trigger = pc_entry->data.addrHistory[pc_entry->data.lookahead + 1];

            if (addr_before_trigger != 0 && addr_before_trigger != trigger_addr) {
              conflictTableKey = addr_before_trigger ^ (trigger_addr << 5) ^ (trigger_addr >> 7);
              conflictTableKey ^= conflictTableKey >> 16;

              conflict_meta_entry = conflictMetaTable->find(conflictTableKey);
              if (conflict_meta_entry) {
                mainMetaTable->insert(trigger_addr, pc, {conflict_meta_entry->data.target_addr});
              }
              auto victim_conflict_meta_entry = conflictMetaTable->insert(conflictTableKey, {block_addr});
              if (!victim_conflict_meta_entry.valid || victim_conflict_meta_entry.key != conflictTableKey)
                conflictMetaTable->set_default(conflictTableKey);
              else
                conflictMetaTable->touch(conflictTableKey);
            }
#endif
          }
        } else {
          mainMetaTable->insert(trigger_addr, pc, {block_addr});
        }
      }

      // update PC table entry
      if (pc_entry->data.addrHistory.front() != block_addr) {
        pc_entry->data.modified = true;
      }
      if (pc_entry->data.addrHistory.size() > pc_entry->data.lookahead + 1) {
        pc_entry->data.addrHistory.pop_back();
      }
      pc_entry->data.addrHistory.push_front(block_addr);

    } else {
      train_pc_meta_table = true;
    }
  } else {
    train_pc_meta_table = true;
    auto victim_pc_entry = pcTable->insert(pc, {block_addr, cache_hit});
#ifdef PC_TRIGGER_PREFETCHING
    if (!resized && !llc_cache->warmup && victim_pc_entry.valid && !victim_pc_entry.data.modified) {
      unmodified_PC++;
    }
    if (!resized && !llc_cache->warmup) {
      inserted_PC++;
    }
#endif
  }

#ifdef PC_TRIGGER_PREFETCHING
  // train PC Meta Table
  if (!resized && !llc_cache->warmup && !cache_hit && train_pc_meta_table) {
    lack_trigger_miss++;
  }
  if (!resized && !llc_cache->warmup && train_pc_meta_table) {
    lack_trigger++;
  }
  if (enable_PC_Trigger_prefetching && train_pc_meta_table) {
    if (!cache_hit && PCQ.size() > 0) {
      uint64_t triggerIP = *PCQ.rbegin();
      if (triggerIP) {
        uint64_t pc_meta_table_key = hash_xor(triggerIP);
        auto victim_pc_meta_entry = pcMetaTable->insert(pc_meta_table_key, {block_addr});
        if (!victim_pc_meta_entry.valid || victim_pc_meta_entry.key != pc_meta_table_key)
          pcMetaTable->set_default(pc_meta_table_key);
        else
          pcMetaTable->touch(pc_meta_table_key);
      }
    }
  }

  // prefetches from PC Meta Table
  if (enable_PC_Trigger_prefetching) {
    uint64_t lookupPC = hash_xor(pc);
    auto pc_meta_entry = pcMetaTable->find(lookupPC);
    if (pc_meta_entry) {
      pcMetaTable->touch(lookupPC);
#ifdef PREFETCH_FILTER
      if (!pf_filter->find(pc_meta_entry->data.target_addr))
#endif
      {
        prefetch_line({pc_meta_entry->data.target_addr << LOG2_BLOCK_SIZE}, true, 0);
#ifdef PREFETCH_FILTER
        pf_filter->insert(pc_meta_entry->data.target_addr);
#endif
        PCM_issued_prefetches.insert(pc_meta_entry->data.target_addr);
      }
    }
  }

  // update PCQ
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
#endif

  return metadata_in;
}

uint32_t prism::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in,
                                      champsim::address ip)
{
#ifdef PREFETCH_FILTER
  pf_filter->erase((evicted_addr.to<uint64_t>()) >> LOG2_BLOCK_SIZE);
#endif

#ifdef PC_TRIGGER_PREFETCHING
  if (PCM_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_filled_prefetches.insert(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    PCM_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (PCM_filled_prefetches.count(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_useless_prefetches++;
    PCM_filled_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
#endif

#ifdef CONFLICT_PREFETCHING
  conflict_table_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
#endif

  return 0;
}

void prism::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where) {}

void prism::prefetcher_final_stats()
{
#ifdef ELABORATE_LOG
  logfile.close();
#endif

  cout << "MT_lookups " << main_table_lookups << endl;
  cout << "MT_hits " << main_table_hits << endl;
  cout << "MT_hitrate " << (main_table_lookups ? (double)main_table_hits / main_table_lookups : 0) << endl;
  cout << "MT_issuedpf " << main_table_issued_prefetches << endl;
  cout << "MT_accuratepf " << main_table_accurate_prefetches << endl;
  cout << "MT_accuracy " << (main_table_issued_prefetches ? (double)main_table_accurate_prefetches / main_table_issued_prefetches : 0) << endl;

#ifdef PC_TRIGGER_PREFETCHING
  cout << "PCM_late_prefetches " << PCM_late_prefetches << endl;
  cout << "PCM_useful_prefetches " << PCM_useful_prefetches << endl;
  cout << "PCM_useless_prefetches " << PCM_useless_prefetches << endl;
  uint64_t PCM_accurate_prefetches = PCM_late_prefetches + PCM_useful_prefetches;
  uint64_t PCM_total_prefetches = PCM_late_prefetches + PCM_useful_prefetches + PCM_useless_prefetches;
  cout << "PCM_accuracy " << (PCM_total_prefetches ? (double)PCM_accurate_prefetches / PCM_total_prefetches : 0) << endl;
  cout << "PCM_laterate " << (PCM_accurate_prefetches ? (double)PCM_late_prefetches / PCM_accurate_prefetches : 0) << endl;

  cout << "Unmod_PC " << unmodified_PC << endl;
  cout << "Insert_PC " << inserted_PC << endl;
  cout << "Lack_Tri " << lack_trigger << endl;
  cout << "Lack_TriMiss " << lack_trigger_miss << endl;
#endif

#ifdef CONFLICT_PREFETCHING
  cout << "CT_lookups " << conflict_table_lookups << endl;
  cout << "CT_hits " << conflict_table_hits << endl;
  cout << "CT_hitrate " << (conflict_table_lookups ? (double)conflict_table_hits / conflict_table_lookups : 0) << endl;
  cout << "CT_issuedpf " << conflict_table_issued_prefetches << endl;
  cout << "CT_accuratepf " << conflict_table_accurate_prefetches << endl;
  cout << "CT_accuracy " << (conflict_table_issued_prefetches ? (double)conflict_table_accurate_prefetches / conflict_table_issued_prefetches : 0) << endl;
#endif

#ifdef INIT_RESIZE
  cout << "Resize_L3Hit_rate " << resize_llc_hit_rate << endl;
  cout << "Resize_UPF_rate " << resize_useful_prefetch_rate << endl;
  cout << "Resize_Score " << resize_score << endl;
  cout << "Resize_WayForCache_" << waysForCache << endl;
#endif
}

void prism::prefetcher_cycle_operate() {}

bool prismMetaTable::insert(uint64_t key, uint64_t ip, const MetaTableEntry& data)
{
  reverse_metatable[data.target_addr].insert(key);

  Entry victim_entry = Super::insert(key, data);
#ifdef TOUCH_FOR_TRIGGER
  if (!victim_entry.valid || victim_entry.key != key)
    Super::set_default(key, ip);
#else
  Super::set_default(key, ip);
#endif

  bool ret = false;
  if (victim_entry.valid) {
    reverse_metatable[victim_entry.data.target_addr].erase(victim_entry.key);
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

void prismMetaTable::insert_for_resize(uint64_t key, const MetaTableEntry& data, uint64_t rrpv_value)
{
  reverse_metatable[data.target_addr].insert(key);
  auto victim_entry = Super::insert(key, data);
  set_rrpv(key, rrpv_value);
  if (victim_entry.valid) {
    reverse_metatable[victim_entry.data.target_addr].erase(victim_entry.key);
  }
}

uint64_t hash_xor(uint64_t key) { return key ^ (key >> 7) ^ (key >> 13) ^ (key >> 21); }