#include "triangel.h"

uint32_t triangel::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                                    uint32_t metadata_in)
{
  if (!warmup_reset && !llc_cache->warmup) {
    reset_stat_counter();
  }
  std::vector<uint64_t> prefetch_addresses;
  second_chance_timestamp++;
  global_timestamp++;
  uint64_t line_addr = (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE); // Line addr
  uint64_t pc = ip.to<uint64_t>();
  if (ip.to<uint64_t>() == 0 || type == access_type::WRITE) {
    SD->TryCacheHit(line_addr); // only check cache hit for set dueling
    return metadata_in;        // Ignore if no IP info
  }


  TrainingUnitEntry* TU_entry = TU->get(pc);
  /* Energy */
  TU->read_n++;

  bool should_pf = false;
  bool should_sample = false;
  bool found_correlated_addr = false;

  uint64_t last_addr = 0;
  uint64_t target = 0;

  /* Step1. Access Train Unit (TU) */
  if (TU_entry) {
    /* Energy */
    TU->write_n++;
    found_correlated_addr = true;
    last_addr = TU_entry->last_addr0;
    

    /* update A.Lookahead based on 1.(A.highPatternConf > threshold); 2.(A.lowPatternConf < threshold) */
    if (TU_entry->pattern_conf1 >= 14)
      TU_entry->currently_twodist_pf = true;

    if (TU_entry->pattern_conf0 < 0)
      TU_entry->currently_twodist_pf = false;

    /* Use lastaddr or lastlastaddr? Based on A.lookahead */
    if (TU_entry->currently_twodist_pf)
      last_addr = TU_entry->last_addr1;
    if (line_addr == last_addr)
      return metadata_in; // to avoid repeat trainings.
    target = line_addr;

    /* Do prefetching only when A.reuseConf and A.lowPatternConf satisfy the conditions */
    should_pf = (TU_entry->reuse_conf > (global_reuse_conf > 64 ? 7 : 8)) && (TU_entry->pattern_conf0 > (global_pattern_conf0 > 64 ? 7 : 8));
  } else {
    bool high_conf = global_reuse_conf > 64 && global_pattern_conf0 > 64 && global_pattern_conf1 > 64;
    if (RandomChance(8, 8) && !high_conf) {
      should_sample = true;
    }
    /* If current globalReuseConf, globalPatternConf and globalHighPatternConf > 64, Insert (CurPC, Addr) into Training Unit*/
    if (should_sample || high_conf) {
      auto TU_new = TrainingUnitEntry(pc, line_addr, global_timestamp); /* New TU Entry (PC, Addr) */
      if (global_pattern_conf1 > 96)
        TU_new.currently_twodist_pf = true;
      TU->set(pc, TU_new);
      /* Energy */
      TU->write_n++;
    }
  }

  /* Step2. Access Second Chance Sampler (SCS) if we found correlated addr in Step1 */
  if (found_correlated_addr) {
    SecondChanceSamplerEntry* SC_entry = SC->find(line_addr); /* Search the entry in Second Chance Sampler */
    /* Energy */
    SC->read_n++;
    /* If found SCS Entry and it is never used */
    if (SC_entry && !SC_entry->used) {
      /* Energy */
      SC->write_n++;
      SC_entry->used = true;
      /* Found the TU Entry corresponding to this SCS Entry */
      TrainingUnitEntry* TU_entry = TU->find(SC_entry->train_pc);
      /* Energy */
      TU->read_n++;
      if(TU_entry) {
        /* Energy */
        TU->write_n++;
      }
     
      if (TU_entry && SC_entry->timestamp + 512 > second_chance_timestamp) {
        if (SC_entry->train_pc == pc) {
          TU_entry->pattern_conf0.increment();
          TU_entry->pattern_conf1.increment();
          global_pattern_conf0.increment();
          global_pattern_conf1.increment();
        }
      } else if (TU_entry) {
        TU_entry->pattern_conf0.decrement(2);
        TU_entry->pattern_conf1.decrement(5);
        global_pattern_conf0.decrement(2);
        global_pattern_conf1.decrement(5);
      }
    }

    /* Step3. Check History Sampler for the corresponding TU Entry */
    HistorySamplerEntry* HS_entry = HS->find(TU_entry->last_addr0);
    /* Energy */
    HS->read_n++;
    if (HS_entry && HS_entry->tu_entry_key == TU_entry->key) {
      /* Energy */
      HS->write_n++;
          /* If T.timestamp - A.timestamp is lower than a threshold, increase T.reuseConf and global reuseConf */
      int64_t time_distance = TU_entry->local_timestamp - HS_entry->timestamp;
      if (time_distance > 0 && time_distance < 196608 * 2) { // magic nonsense number
        TU_entry->reuse_conf.increment();
        global_reuse_conf.increment();
      } else if (!HS_entry->reused) {
        TU_entry->reuse_conf.decrement();
        global_reuse_conf.decrement();
      }
      HS_entry->reused = true;

      /* If A.Target == Addr, increase local and global highPatternConf, basePatternConf */
      bool will_be_confident = (line_addr == HS_entry->target_addr);

      if (will_be_confident) {
        TU_entry->pattern_conf0.increment();
        TU_entry->pattern_conf1.increment();
        global_pattern_conf0.increment();
        global_pattern_conf1.increment();
      }
      else if(cache_pf_map.find(HS_entry->target_addr) != cache_pf_map.end() && !cache_pf_map[HS_entry->target_addr]) {
        /* If A.Target != Addr and A.Target is not in Second Chance Sampler, insert A.Target into Second Chance Sampler to wait for Second Chance Pattern */
        SecondChanceSamplerEntry* SC_entry = SC->get_victim(HS_entry->target_addr);
        /* Energy */
        SC->read_n++;
        if (SC_entry && !SC_entry->used) {
          /* 
             If the entry is evicted before ever being used, it means the expected
             second-chance pattern has not appeared for a long time. In this case,
             treat the memory-access pattern at V.Train-Idx as weak and decrease
             both global and local PatternConf counters asymmetrically.
          */
          TrainingUnitEntry* TU_entry = TU->find(SC_entry->train_pc);
          /* Energy */
          TU->read_n++;
          if (TU_entry) {
            /* Energy */            
            TU->write_n++;
            TU_entry->pattern_conf0.decrement(2);
            TU_entry->pattern_conf1.decrement(5);
            global_pattern_conf0.decrement(2);
            global_pattern_conf1.decrement(5);
          }
        }
        auto new_SC_entry = SecondChanceSamplerEntry(HS_entry->target_addr, pc, second_chance_timestamp); // used default to false
        SC->set(HS_entry->target_addr, new_SC_entry);
        /* Energy */
        SC->write_n++;
      }

      /* 使用地址对 (PrevAddr, Addr) 更新 History Sampler 的采样 A ： A.Target = Addr */
      if (HS_entry->tu_entry_key == TU_entry->key) {
        HS_entry->target_addr = line_addr;
      }
      HS_entry->confident = will_be_confident;
    } 
    else if (should_sample || RandomChance(TU_entry->reuse_conf.value, TU_entry->sample_rate.value)) {
      /* If no valid sample is found, or the sample belongs to another PC's address stream,
         sample CurPC's address pair (PrevAddr, Addr) with a random probability. */
      auto HS_entry = HS->get_victim(TU_entry->last_addr0);
      /* Energy */
      HS->read_n++;
      if (HS_entry) {
        auto TU_entry_from_hs = TU->find(HS_entry->tu_entry_key);
        /* Energy */
        TU->read_n++;
        if (TU_entry_from_hs) {

          uint64_t distance = TU_entry_from_hs->local_timestamp - HS_entry->timestamp; // ! UAF
           /* If the timestamp gap between V and the Training Unit entry for V's sampled PC
             exceeds a time window, the sample is considered too stale, so increase
             CurPC's sampling rate. */
          if (distance > 196608 * 2) {
            TU->touch(TU_entry_from_hs->key);
            /* If additionally !V.Accessed, sample V is stale and has remained unused
              for a long time. That implies the address stream at V.Train-Idx is
              unlikely to revisit V's recorded address soon, so decrease
              V.Train-Idx's reuseConf. */
            if (!HS_entry->reused) {

              TU_entry_from_hs->reuse_conf.decrement();
              /* Energy */
              TU->write_n++;
              global_reuse_conf.decrement();
            }
            TU_entry->sample_rate.increment();
          } else if (distance > 0 && !HS_entry->reused) {
            /* If the timestamp gap is within a time window and !V.Accessed, the entry
               was resampled before repeated addresses in the stream could be observed.
               Therefore, decrease CurPC's sampling rate to avoid overly frequent
               replacement. */
            TU_entry->sample_rate.decrement();
          }
        } else {
          // HS_entry is already the victim and is being replaced now.
        }

      } else {
        TU_entry->sample_rate.increment();
      }

      auto HS_new = HistorySamplerEntry(TU_entry->last_addr0, TU_entry->key, line_addr, TU_entry->local_timestamp + 1);
      HS->set(TU_entry->last_addr0, HS_new);
      /* Energy */
      HS->write_n++;
    }
  }

  /* Step4. Update Size Dueller */
  SD->TryCacheHit(line_addr); // just mantain the size dueller
  if (should_pf) {
    SD->TryMarkovHit(line_addr);
  }

  /* Step5. Update LLC partition */
  if (global_timestamp > 50000000) {
    /* Find the maximum value Hits[m] among all Hits counters. */
    int target_size = SD->Duel();
    uint64_t target_score = SD->dueller_counters[target_size];
    uint64_t current_score = SD->dueller_counters[current_partition];
    /* If Hits[cur_size] is less than 4/5 of Hits[m], repartition and set the
       target partition size to m (number of cache lines). */
    if (target_size != (current_partition) && target_score > current_score * 1.25) {
      current_partition = target_size;
      MD->repartition(current_partition);
      llc_cache->set_available_ways(16 - current_partition);
    }

    global_timestamp = 0;
    SD->ResetCounters();
  }

  /* Step6. Train Markov table */
  if (found_correlated_addr && should_pf && current_partition > 0) {
    AddMetadata(last_addr, target);
    MT_inserts++;
  }

  /* Step7. Issue prefetches */
  if (target != 0 && should_pf && current_partition > 0) {
    bool find = false;
    MT_lookup_reqs++;
    
    auto MD_entry = GetMetadata(target, true);
    MT_lookups++;
    if (MD_entry) {
      find = true;
      MT_hits++;
    }
    if (MD_entry) {
      MD_entry->used = true;
    }
    int degree = 0;
    bool high_degree_pf = MD_entry && (TU_entry->pattern_conf1 > (global_pattern_conf1 > 64 ? 7 : 8));
    int max_degree = high_degree_pf ? MAX_DEGREE : (should_pf ? 1 : 0);
    while (MD_entry && degree < max_degree) {
      uint64_t PF_addr = MD_entry->target_addr;
      prefetch_addresses.push_back(PF_addr);
      degree++;
      if (degree < max_degree) {
        MD_entry = GetMetadata(PF_addr, true);
        MT_lookups++;
        if(MD_entry) {
          find = true;
          MT_hits++;
        }
      } else {
        MD_entry = nullptr;
      }
    }
    if(find) MT_lookup_returns++;
  }
  removeDuplicates(prefetch_addresses);
  for (size_t i = 0; i < prefetch_addresses.size(); i++) {
    uint64_t p_addr = prefetch_addresses[i] << LOG2_BLOCK_SIZE;
    if (p_addr == 0)
      break;
    champsim::address prefetch_addr{p_addr};
    const bool success = prefetch_line(prefetch_addr, true, 0);
  }

  /* Step8. Update Train Unit */
  if (TU_entry) {
    TU_entry->last_addr1 = TU_entry->last_addr0;
    TU_entry->last_addr0 = line_addr;
    TU_entry->local_timestamp++;
  }

  return metadata_in;
}

uint32_t triangel::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr,
                                                 uint32_t metadata_in)
{
  uint64_t line_addr = addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
  uint64_t evaddr = evicted_addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
  cache_pf_map[line_addr] = prefetch ? true:false;
  
  if(evaddr != 0) {
    cache_pf_map.erase(evaddr);
  }
  return metadata_in;
}

void triangel::prefetcher_final_stats() {
  cout << "MT_lookups " << MT_lookups << endl;
  cout << "MT_hits " << MT_hits << endl;
  cout << "MT_lookup_reqs " << MT_lookup_reqs << endl;
  cout << "MT_lookup_returns " << MT_lookup_returns << endl;
  cout << "MT_inserts " << MT_inserts << endl;

  cout << "history_sampler_read " << HS->read_n << endl;
  cout << "history_sampler_write " << HS->write_n << endl;
  cout << "second_chance_sampler_read " << SC->read_n << endl;
  cout << "second_chance_sampler_write " << SC->write_n << endl;
  cout << "training_unit_read " << TU->read_n << endl;
  cout << "training_unit_write " << TU->write_n << endl;
  cout << "reuse_buffer_read " << RB->read_n << endl;
  cout << "reuse_buffer_write " << RB->write_n << endl;
  cout << "markov_table_read " << MD->read_n << endl;
  cout << "markov_table_write " << MD->write_n << endl;
}

void triangel::prefetcher_cycle_operate() {}