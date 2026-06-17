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

  /* 1. 访问 Training Unit */
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
    /* 若当前 globalReuseConf, globalPatternConf 和 globalHighPatternConf 都大于默认值（64），则将 (CurPC, Addr) 作为表项插入 Training Unit*/
    if (should_sample || high_conf) {
      auto TU_new = TrainingUnitEntry(pc, line_addr, global_timestamp); /* 分配一个新的 TU Entry (PC, Addr) */
      if (global_pattern_conf1 > 96)
        TU_new.currently_twodist_pf = true;
      TU->set(pc, TU_new);
      /* Energy */
      TU->write_n++;
    }
  }

  /* Step2. Access Second Chance Sampler (SCS) if we found correlated addr in Step1 */
  if (found_correlated_addr) {
    SecondChanceSamplerEntry* SC_entry = SC->find(line_addr); /* 在 Second Chance Sampler 中查找当前地址对应的 Entry */
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
      /* 若 T.timestamp 和 A.timestamp 相差小于一个时间窗口，则认为 CurPC 处指令访存重复特征显著，增大 T.reuseConf 和全局 reuseConf ，反之，减少局部和全局的
       * reuseConf */
      int64_t time_distance = TU_entry->local_timestamp - HS_entry->timestamp;
      if (time_distance > 0 && time_distance < 196608 * 2) { // magic nonsense number
        TU_entry->reuse_conf.increment();
        global_reuse_conf.increment();
      } else if (!HS_entry->reused) {
        TU_entry->reuse_conf.decrement();
        global_reuse_conf.decrement();
      }
      HS_entry->reused = true;

      /* 如果 A.Target == Addr ，则说明 PC 处地址流出现地址对 (PrevAddr, Addr)
         的重复，认为 CurPC 处指令访存模式特征显著，增大局部和全局的 highPatternConf 以
         及 basePatternConf */
      bool will_be_confident = (line_addr == HS_entry->target_addr);

      if (will_be_confident) {
        TU_entry->pattern_conf0.increment();
        TU_entry->pattern_conf1.increment();
        global_pattern_conf0.increment();
        global_pattern_conf1.increment();
      }
      // TODO: L2 cache has HS_entry->target_addr and it was not prefetched
      else if(cache_pf_map.find(HS_entry->target_addr) != cache_pf_map.end() && !cache_pf_map[HS_entry->target_addr]) {
        /* 若 A.Target != Addr 且 Second Chance Sampler 中没有 A.Target ，则将 A.Target 插入 Second Chance Sampler 以等待 Second Chance Pattern */
        SecondChanceSamplerEntry* SC_entry = SC->get_victim(HS_entry->target_addr);
        /* Energy */
        SC->read_n++;
        if (SC_entry && !SC_entry->used) {
          /* 如果被驱逐时还未被使用过，则说明过了很久还没有出现希望看到的 Second Chance Pattern，此时认为 V.Train-Idx 处指令访存模式特征不显著，不对
             称的降低全局的和局部的 PatternConf 计数器
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
      /* 若未找到有效采样或采样来自另一个 PC 的地址流，则以随机概率 采样 CurPC 的地址对 (PrevAddr, Addr) */
      auto HS_entry = HS->get_victim(TU_entry->last_addr0);
      /* Energy */
      HS->read_n++;
      if (HS_entry) {
        auto TU_entry_from_hs = TU->find(HS_entry->tu_entry_key);
        /* Energy */
        TU->read_n++;
        if (TU_entry_from_hs) {

          uint64_t distance = TU_entry_from_hs->local_timestamp - HS_entry->timestamp; // ! UAF
          /* 若 V 的时间戳和 V 采样的 PC 对应的 Training Unit 表项的时间戳相差大于一个时间窗口，则说
             明采样过于陈旧，因此增大 CurPC 的采样率： */
          if (distance > 196608 * 2) {
            TU->touch(TU_entry_from_hs->key);
            /* 若在此基础上 !V.Accessed ， 说明采样 V 过于陈旧且长时间没被使用；意味着
               V.Train-Idx 的地址流长时间不会出现采样 V 记录的地址，因此降低 V.Train-Idx 的 reuseConf */
            if (!HS_entry->reused) {

              TU_entry_from_hs->reuse_conf.decrement();
              /* Energy */
              TU->write_n++;
              global_reuse_conf.decrement();
            }
            TU_entry->sample_rate.increment();
          } else if (distance > 0 && !HS_entry->reused) {
            /* 若时间戳相差小于一个时间窗口，且 !V.Accessed ，说明采样后还没来得及发现地址流中的重复地
               址就被重新采样，因此需要降低 CurPC 的采样率。避免过于频繁的替换。*/
            TU_entry->sample_rate.decrement();
          }
        } else {
          // HS_entry 本来就是 victim,刚好被替换掉。
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
    /* 找到 Hits 计数器里最大的 Hits[m] */
    int target_size = SD->Duel();
    uint64_t target_score = SD->dueller_counters[target_size];
    uint64_t current_score = SD->dueller_counters[current_partition];
    /* 若当前分区大小 cur_size 对应的 Hits[cur_size] 小于 Hits[m] 的 4/5，则调整分区，且目标分区大小为 m （Cache Line 数） */
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
    
    /* 使用 Addr 索引 Markov 分区得到预取目标地址 TargetAddr0 */
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