#include "prism.h"

int prism::issue_mainMetatable(uint64_t pc, uint64_t block_addr, int degree)
{
  int cur = 0;
  uint64_t lookup = block_addr;
  auto pc_entry = pcTable->find(pc);

#ifdef PREFETCH_FILTER
  auto filter_entry = pf_filter->find(lookup);
  energy_stats.filter_table_read++;
  while (filter_entry) {
    lookup = filter_entry->next_addr;
    cur++;

    if (cur >= degree) {
      return 0;
    }
    filter_entry = pf_filter->find(lookup);
    energy_stats.filter_table_read++;
  }
#endif
  pat_lookup_times[cur + 1]++;

  int i;
  for (i = cur; i < degree; i++) {
    auto entry = mainMetaTable->find(lookup);
    MT_lookups++;
    energy_stats.markov_read[waysForMarkov == 1 ? 0 : 1]++;
    if (entry == nullptr)
      break;
    MT_hits++;
    bool success;
    success = prefetch_line({entry->target_addr << LOG2_BLOCK_SIZE}, true, 0);
    if (success) {
      pc_entry->data.issuedPrefetchCount++;
      if (!llc_cache->warmup) {
        resize_issued_prefetch++;
      }
      pf_filter->insert(lookup, entry->target_addr, true);
      energy_stats.filter_table_write++;
    } else {
      return i;
    }

    lookup = entry->target_addr;

    // stat
    if (success) {
      if (main_table_prefetches.find(entry->target_addr) == main_table_prefetches.end()) {
        main_table_issued_prefetches++;
        main_table_prefetches.insert(entry->target_addr);
      }
    }
  }

  return i;
}

uint32_t prism::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                         uint32_t metadata_in, std::string latepf)
{
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

  auto pc_entry = pcTable->find(pc);
  energy_stats.training_table_read++;
  bool train_pc_meta_table = false;
  if (pc_entry) {
    // update pc entry
    energy_stats.training_table_write++;
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
#ifdef DYNAMIC_DEGREE
      int cur_degree = pc_entry->data.degree;
      // For cases where the degree is 0, we will issue a prefetch every 8 misses
      if (cur_degree <= 0) {
        if (discard_prefetch == 0) {
          cur_degree = 1;
        }
        ++discard_prefetch;
        if (discard_prefetch == 8)
          discard_prefetch = 0;
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
              // update RRPV value when inserting a same entry
              mainMetaTable->touch(trigger_addr, pc);
#endif
            } else {
              // metadata conflict
#ifdef INSERTION_POLICY
              if (pc_entry->data.hitCount < 7)
#endif
              {
                mainMetaTable->insert(trigger_addr, pc, {block_addr});
                MT_inserts++;
                energy_stats.markov_write[waysForMarkov == 1 ? 0 : 1]++;
              }
            }
          } else {
#ifdef INSERTION_POLICY
            if (pc_entry->data.hitCount < 7)
#endif
            {
              mainMetaTable->insert(trigger_addr, pc, {block_addr});
              MT_inserts++;
              energy_stats.markov_write[waysForMarkov == 1 ? 0 : 1]++;
            }
          }
        } else if (pc_entry->data.degree == 0) {
          // For cases where the degree is 0, we will insert metadata every 8 times
          ++discard_metadata;
          if (discard_metadata == 8)
            discard_metadata = 0;
        }
      } else {
        train_pc_meta_table = true;
      }

      // update PC table entry
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
    inserted_PC++;
  }
  if (train_pc_meta_table) {
    fail_entry_init++;
  }

#ifdef PC_TRIGGER_PREFETCHING
  // train PC Meta Table
  if (enable_PC_Trigger_prefetching && train_pc_meta_table) {
    if (PCQ.size() > 0) {
      uint64_t triggerIP = *PCQ.rbegin();
      if (triggerIP) {
        uint64_t pc_meta_table_key = hash_xor(triggerIP);
#ifdef INF_PAT
        pcMetaTable[pc_meta_table_key] = {block_addr};
#else
        pcMetaTable->insert(pc_meta_table_key, {block_addr});
#endif
        PAT_inserts++;
        energy_stats.pat_write[waysForPAT == 1 ? 0 : 1]++;
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

      PAT_lookups++;
      energy_stats.pat_read[waysForPAT == 1 ? 0 : 1]++;
#ifdef INF_PAT
      auto pc_meta_entry = pcMetaTable.find(lookupPC);
      if (pc_meta_entry != pcMetaTable.end()) {
        uint64_t target_addr = pc_meta_entry->second.target_addr;
#else
      auto pc_meta_entry = pcMetaTable->find(lookupPC);
      if (pc_meta_entry) {
        uint64_t target_addr = pc_meta_entry->target_addr;
#endif
        PAT_hits++;
#ifdef PREFETCH_FILTER
        if (!pf_filter->find(target_addr))
#endif
        {
          prefetch_line({target_addr << LOG2_BLOCK_SIZE}, true, 0);
#ifdef PREFETCH_FILTER
          pf_filter->insert(target_addr, 0);
#endif
          PCM_issued_prefetches.insert(target_addr);
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
  // only for stat
  if (PCM_issued_prefetches.count(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_filled_prefetches.insert(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    PCM_issued_prefetches.erase(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
  if (PCM_filled_prefetches.count(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE)) {
    PCM_useless_prefetches++;
    PCM_filled_prefetches.erase(evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
  }
#endif
  return 0;
}

void prism::prefetcher_late_prefetch(champsim::address addr, champsim::address ip, std::string where) {}

void prism::prefetcher_final_stats()
{
  cout << "MT_lookups " << MT_lookups << endl;
  cout << "MT_hits " << MT_hits << endl;
  cout << "MT_inserts " << MT_inserts << endl;
  cout << "PAT_lookups " << PAT_lookups << endl;
  cout << "PAT_hits " << PAT_hits << endl;
  cout << "PAT_inserts " << PAT_inserts << endl;

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
  cout << "Fail_Entry_Init " << init_fail_entry_init << endl;
  cout << "Insert_PC " << init_insert_PC << endl;
#endif

  cout << "Init_L3Hit_rate " << init_resize_llc_hit_rate << endl;
  cout << "Init_UPF_rate " << init_resize_useful_prefetch_rate << endl;
  cout << "Init_Score " << init_resize_score << endl;
  cout << "Init_WayForMarkov " << init_ways_for_markov << endl;
  cout << "Init_WayForPAT " << init_ways_for_pc_metadata_table << endl;

  cout << "training_unit_read " << energy_stats.training_table_read << endl;
  cout << "training_unit_write " << energy_stats.training_table_write << endl;
  cout << "1_way_markov_table_read " << energy_stats.markov_read[0] << endl;
  cout << "1_way_markov_table_write " << energy_stats.markov_write[0] << endl;
  cout << "8_way_markov_table_read " << energy_stats.markov_read[1] << endl;
  cout << "8_way_markov_table_write " << energy_stats.markov_write[1] << endl;
  cout << "filter_table_read " << energy_stats.filter_table_read << endl;
  cout << "filter_table_write " << energy_stats.filter_table_write << endl;
  cout << "1_way_pat_table_read " << energy_stats.pat_read[0] << endl;
  cout << "1_way_pat_table_write " << energy_stats.pat_write[0] << endl;
  cout << "2_way_pat_table_read " << energy_stats.pat_read[1] << endl;
  cout << "2_way_pat_table_write " << energy_stats.pat_write[1] << endl;
  for (int i = 1; i < 7; i++) {
    cout << "pat_lookup_times_" << i << " " << pat_lookup_times[i] << endl;
  }
}

void prism::prefetcher_cycle_operate() {}

uint64_t hash_xor(uint64_t key) { return key ^ (key >> 7) ^ (key >> 13) ^ (key >> 21); }