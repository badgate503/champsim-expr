#include "prism.h"

int prism::issue_mainMetatable(uint64_t pc, uint64_t block_addr, int degree)
{
  int cur = 0;
  uint64_t lookup = block_addr;
  auto pc_entry = pcTable->find(pc);
  pf_filter_entry *last_filter_entry = nullptr, *cur_filter_entry = nullptr;

#ifdef PREFETCH_FILTER
  auto filter_entry = pf_filter->find(lookup);
  if (filter_entry && filter_entry->next) {
    filter_entry = filter_entry->next;
    while (filter_entry) {
#ifdef MULTI_LEVEL_PREFETCH
      if (!filter_entry->fill_l2 && cur < 3) {
        bool success = prefetch_line({filter_entry->pf_addr << LOG2_BLOCK_SIZE}, true, 0);
        if (success) {
          pc_entry->data.issuedPrefetchCount++;
          if (!llc_cache->warmup) {
            resize_issued_prefetch++;
          }
        }
      }
#endif
      lookup = filter_entry->pf_addr;
      last_filter_entry = filter_entry;
      filter_entry = filter_entry->next;
      cur++;
      if (cur > degree) {
        return 0;
      }
    }
  }
#endif

  int i;
  for (i = cur; i < degree; i++) {
    auto entry = mainMetaTable->find(lookup);
    MT_lookups++;
    if (entry == nullptr)
      break;
    MT_hits++;
    bool success;
#ifdef MULTI_LEVEL_PREFETCH
    if (i < 3) {
      success = prefetch_line({entry->target_addr << LOG2_BLOCK_SIZE}, true, 0);
      if (success) {
        pc_entry->data.issuedPrefetchCount++;
        if (!llc_cache->warmup) {
          resize_issued_prefetch++;
        }
        cur_filter_entry = pf_filter->insert(entry->target_addr, true);
      } else {
        return i;
      }
    } else {
      success = llc_cache->prefetch_line({entry->target_addr << LOG2_BLOCK_SIZE}, true, 0);
      if (success) {
        cur_filter_entry = pf_filter->insert(entry->target_addr, false);
      } else {
        return i;
      }
    }
#else
    success = prefetch_line({entry->target_addr << LOG2_BLOCK_SIZE}, true, 0);
    if (success) {
      pc_entry->data.issuedPrefetchCount++;
      if (!llc_cache->warmup) {
        resize_issued_prefetch++;
      }
      cur_filter_entry = pf_filter->insert(entry->target_addr, true);
    } else {
      return i;
    }
#endif
    if (last_filter_entry) {
      last_filter_entry->next = cur_filter_entry;
    }
    last_filter_entry = cur_filter_entry;
    lookup = entry->target_addr;

    // stat
    if (success) {
      if (main_table_prefetches.find(entry->target_addr) != main_table_prefetches.end()) {
        main_table_issued_prefetches++;
        main_table_prefetches.insert(entry->target_addr);
      }
    }
  }

  return i;
}

// int prism::issue_mainMetatable(uint64_t pc, uint64_t block_addr, int degree)
// {
//   int issued = 0;
//   uint64_t lookup = block_addr;
//   auto pc_entry = pcTable->find(pc);
//   for (int i = 0; i < degree; i++) {
//     auto entry = mainMetaTable->find(lookup);
//     if (entry == nullptr)
//       break;
// #ifdef PREFETCH_FILTER
//     if (pf_filter->find(entry->target_addr)) {
//       lookup = entry->target_addr;
//       continue;
//     }
// #endif

//     const bool success = prefetch_line({entry->target_addr << LOG2_BLOCK_SIZE}, true, 0);
//     if (success) {
//       pc_entry->data.issuedPrefetchCount++;
//       issued++;
//       main_table_issued_prefetches++;
//       main_table_prefetches.insert(entry->target_addr);
//       if (!llc_cache->warmup) {
//         resize_issued_prefetch++;
//       }
//       pf_filter->insert(entry->target_addr, true);
// #ifdef ELABORATE_LOG
//       logfile << std::dec << llc_cache->current_cycle() << " ISSUE MT " << std::hex << pc << " " << (lookup) << " " << (candidate->correlatedAddr) <<
//       std::endl;
// #endif
//     } else {
//       break;
//     }
//     lookup = entry->target_addr;
//   }
//   return issued;
// }

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

  if (!warmup_reset && !llc_cache->warmup) {
    reset_stat_counters();
  }

  if (pc == 0 || block_addr == 0)
    return metadata_in;
  if (type == access_type::LOAD || type == access_type::RFO)
    demand++;

  // resize
  if (!llc_cache->warmup && !init_sample) {
    init_sample = true;
    sample_start = true;
    reset_for_sample();
  }
  if (!llc_cache->warmup && demand >= SAMPLE_INTERVAL) {
    sample_start = true;
    reset_for_sample();
  }

  if (sample_start) {
    if (useful_prefetch)
      resize_useful_prefetch++;

    if (demand >= RESIZE_SAMPLE_WINDOW) {
#ifdef PC_TRIGGER_PREFETCHING
      update_pc_metatable_size();
#endif
#ifdef BMP_RESIZE
      update_main_metatable_size();
#endif
      resize_cache();
      sample_start = false;
    }
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
    // update pc entry
    pcTable->set_mru(pc);
    if (cache_hit && !useful_prefetch) {
      pc_entry->data.hitCount++;
    } else {
      pc_entry->data.hitCount = 0;
    }
    if (useful_prefetch) {
      pc_entry->data.usefulPrefetchCount++;
    }
    if (pc_entry->data.issuedPrefetchCount == 64 || pc_entry->data.usefulPrefetchCount == 64) {
      pc_entry->data.update_counter(pc);
    }

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
          pf_filter->insert(conflict_meta_entry->data.target_addr, true);
#endif
          conflict_table_issued_prefetches++;
          conflict_table_prefetches.insert(conflict_meta_entry->data.target_addr);
        }
      }
#endif

#ifdef DYNAMIC_DEGREE
      int cur_degree = pc_entry->data.degree;
      int8_t cur_bw = get_dram_bw();
      if (cur_degree <= 0) {
        if (discard_prefetch == 0) {
          cur_degree = 1;
        }
        ++discard_prefetch;
        if (discard_prefetch == 8)
          discard_prefetch = 0;
      }

      if (cur_bw >= 14) {
        cur_degree = 0;
      } else if (cur_bw >= 12) {
        cur_degree -= 2;
      } else if (cur_bw >= 8) {
        cur_degree -= 1;
      }
#else
      int cur_degree = DEFAULT_DEGREE;
#endif

      if (cur_degree > 0) {
#ifndef REPLACEMENT_POLICY
        auto last_metadata = mainMetaTable->find(block_addr);
        if (last_metadata) {
          mainMetaTable->set_mru(block_addr);
        }
#endif
        issue_mainMetatable(pc, block_addr, cur_degree);
      }

      // training
      uint64_t trigger_addr = 0;
      if (pc_entry->data.addrHistory.size() > pc_entry->data.lookahead)
        trigger_addr = pc_entry->data.addrHistory[pc_entry->data.lookahead];
      // else
      // trigger_addr = pc_entry->data.addrHistory.back();

      if (trigger_addr != 0 && trigger_addr != block_addr) {
        auto exist_metadata = mainMetaTable->find(trigger_addr);
        if (pc_entry->data.degree > 0 || discard_metadata == 0) {
          if (pc_entry->data.degree == 0) {
            discard_metadata++;
          }
          if (exist_metadata) {
            if (exist_metadata->target_addr == block_addr) {
#ifdef REPLACEMENT_POLICY
              mainMetaTable->touch(trigger_addr, pc);
#endif
            } else {
              // metadata conflict
#ifndef CONFLICT_PREFETCHING
#ifdef INSERTION_POLICY
              if (pc_entry->data.hitCount < 7)
#endif
                mainMetaTable->insert(trigger_addr, pc, {block_addr});
#else
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
#ifdef INSERTION_POLICY
            if (pc_entry->data.hitCount < 7)
#endif
              mainMetaTable->insert(trigger_addr, pc, {block_addr});
          }
        } else if (pc_entry->data.degree == 0) {
          ++discard_metadata;
          if (discard_metadata == 8)
            discard_metadata = 0;
        }
      } else {
        train_pc_meta_table = true;
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
    if (!llc_cache->warmup) {
      if (victim_pc_entry.valid && !victim_pc_entry.data.modified) {
        unmodified_PC++;
      }
      inserted_PC++;
    }
  }

#ifdef PC_TRIGGER_PREFETCHING
  // train PC Meta Table
  if (enable_PC_Trigger_prefetching && train_pc_meta_table) {
    if (PCQ.size() > 0) {
      uint64_t triggerIP = *PCQ.rbegin();
      if (triggerIP) {
        uint64_t pc_meta_table_key = hash_xor(triggerIP);
        auto victim_pc_meta_entry = pcMetaTable->insert(pc_meta_table_key, {block_addr});
        PCT_inserts++;
        if (!victim_pc_meta_entry.valid || victim_pc_meta_entry.key != pc_meta_table_key)
          pcMetaTable->set_default(pc_meta_table_key);
        else
          pcMetaTable->touch(pc_meta_table_key);
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
    // update PCQ
    if (PCQ.size() < PCQ_SIZE) {
      PCQ.push_front(pc);
    } else {
      PCQ.pop_back();
      PCQ.push_front(pc);
    }

    // prefetches from PC Meta Table
    if (enable_PC_Trigger_prefetching) {
      uint64_t lookupPC = hash_xor(pc);
      auto pc_meta_entry = pcMetaTable->find(lookupPC);
      PCT_lookups++;
      if (pc_meta_entry) {
        PCT_hits++;
        pcMetaTable->touch(lookupPC);
#ifdef PREFETCH_FILTER
        if (!pf_filter->find(pc_meta_entry->data.target_addr))
#endif
        {
          prefetch_line({pc_meta_entry->data.target_addr << LOG2_BLOCK_SIZE}, true, 0);
#ifdef PREFETCH_FILTER
          pf_filter->insert(pc_meta_entry->data.target_addr, true);
#endif
          PCM_issued_prefetches.insert(pc_meta_entry->data.target_addr);
        }
      }
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

void prism::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where)
{
  auto pc_entry = pcTable->find(ip.to<uint64_t>());
  if (pc_entry) {
    pc_entry->data.latePrefetchCount++;
  }
}

void prism::prefetcher_final_stats()
{
#ifdef ELABORATE_LOG
  logfile.close();
#endif

  cout << "MT_lookups " << MT_lookups << endl;
  cout << "MT_hits " << MT_hits << endl;
  cout << "MT_inserts " << MT_inserts << endl;
  cout << "PCT_lookups " << PCT_lookups << endl;
  cout << "PCT_hits " << PCT_hits << endl;
  cout << "PCT_inserts " << PCT_inserts << endl;

  cout << "MT_hitrate " << (MT_lookups ? (double)MT_hits / MT_lookups : 0) << endl;
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
  cout << "Unmod_PC " << init_unmodified_PC << endl;
  cout << "Insert_PC " << init_insert_PC << endl;
#endif

#ifdef CONFLICT_PREFETCHING
  cout << "CT_lookups " << conflict_table_lookups << endl;
  cout << "CT_hits " << conflict_table_hits << endl;
  cout << "CT_hitrate " << (conflict_table_lookups ? (double)conflict_table_hits / conflict_table_lookups : 0) << endl;
  cout << "CT_issuedpf " << conflict_table_issued_prefetches << endl;
  cout << "CT_accuratepf " << conflict_table_accurate_prefetches << endl;
  cout << "CT_accuracy " << (conflict_table_issued_prefetches ? (double)conflict_table_accurate_prefetches / conflict_table_issued_prefetches : 0) << endl;
#endif

  cout << "Init_L3Hit_rate " << init_resize_llc_hit_rate << endl;
  cout << "Init_UPF_rate " << init_resize_useful_prefetch_rate << endl;
  cout << "Init_NormUPF_rate " << init_resize_norm_upf << endl;
  cout << "Init_Score " << init_resize_score << endl;
  cout << "Init_WayForMarkov " << init_ways_for_markov << endl;
  cout << "Init_WayForPCT " << init_ways_for_pc_metadata_table << endl;
}

void prism::prefetcher_cycle_operate() {}

bool prismMetaTable::insert(uint64_t key, uint64_t ip, const MetaTableEntry& data)
{
  prefetcher->MT_inserts++;
  reverse_metatable[data.target_addr].insert(key);

  Entry victim_entry = Super::insert(key, data);
#ifdef REPLACEMENT_POLICY
  Super::set_default(key, ip);
#else
  Super::set_mru(key);
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
#ifdef REPLACEMENT_POLICY
  Super::set_rrpv(key, rrpv_value);
#else
  Super::set_mru(key);
#endif
  if (victim_entry.valid) {
    reverse_metatable[victim_entry.data.target_addr].erase(victim_entry.key);
  }
}

uint64_t hash_xor(uint64_t key) { return key ^ (key >> 7) ^ (key >> 13) ^ (key >> 21); }