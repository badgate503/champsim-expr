#include "kairos.h"

// vector<kairos> prefetchers = vector<kairos>(NUM_CPUS);

// void kairos::prefetcher_initialize(){}

uint32_t kairos::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                          uint32_t metadata_in)
{
  if (type == access_type::LOAD || type == access_type::RFO) {
    access_count++;
    if (!cache_hit) {
      miss_count++;
    }
    if (useful_prefetch) {
      prefetch_hit_count++;
    }
    if (access_count % TRACKING_WINDOW == 0) {
      evaluate_window();   // update partition
    }

    uint64_t block_addr = (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    uint64_t ip_tag = hash_xor(ip.to<uint64_t>(), 10);
    
    bool is_key_ip = false;
    if (!cache_hit) {
      is_key_ip = detect_unit.update(ip_tag);  // DU for detecting key ip
    }
    
    uint64_t last_addr = 0;
    // Update training unit and get last addr and key tag
    Key_Tag_Type key_tag = train_unit.update(ip_tag, last_addr, block_addr, is_key_ip, cache_hit, useful_prefetch);

    vector<uint64_t> prefetch_candidate;
    if (key_tag == Positive) {
      predict(block_addr, 4, prefetch_candidate);
      if (last_addr != 0 && last_addr != block_addr) {
        metadata.insert(last_addr, block_addr, true);
      }
    } else if (key_tag == Neutral) {
      predict(block_addr, 1, prefetch_candidate);
      if (last_addr != 0 && last_addr != block_addr) {
        metadata.insert(last_addr, block_addr, false);
      }
    } else {
      // Do nothing for Negative or NO_FOUND
    }

    removeDuplicates(prefetch_candidate);

    for (int i = 0; i < prefetch_candidate.size(); i++) {
      uint64_t pf_addr = prefetch_candidate[i] << LOG2_BLOCK_SIZE;
      if (pf_addr == 0)
        break;
      champsim::address prefetch_addr{pf_addr};
      prefetch_line(prefetch_addr, true, 0);
    }
  }
  return metadata_in;
}

uint32_t kairos::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void kairos::prefetcher_final_stats()
{
  cout << "Metadata_Hit: " << metadata_hit_count << '\n';
  cout << "Current metadata size: " << metadata.actual_way << " ways\n";
  cout << "Increase, decrease and maintain metadata numbers: " << increase_metadata << ' ' << decrease_metadata << ' ' << maintain_metadata << '\n';
  cout << "Total windows evaluated: " << current_window << '\n';
}

void kairos::prefetcher_cycle_operate() {}

uint64_t hash_xor(uint64_t key, uint64_t width)
{
  const uint64_t MASK = (1ULL << width) - 1;
  uint64_t hash = 0;
  while (key != 0) {
    hash ^= (key & MASK);
    key >>= width;
  }
  return hash;
}