#include "croage.h"



//vector<Croage> prefetchers = vector<Croage>(NUM_CPUS);

//void croage::prefetcher_initialize(){}

uint32_t croage::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in)
{
  if (type == access_type::LOAD || type == access_type::RFO){

    uint64_t line_addr = (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE); // Line addr

    // Track accesses for metadata sizing
    access_count++;
    if (!cache_hit) {
        miss_count++;
    }
    if (useful_prefetch) {
        prefetch_hit_count++;
    }

    // Check if we've reached the end of a tracking window
    if (access_count % TRACKING_WINDOW == 0) {
      evaluate_window();
  }

    vector<uint64_t> prefetch_candidate = this->predict(ip.to<uint64_t>(), line_addr);
    removeDuplicates(prefetch_candidate);

    this->train(ip.to<uint64_t>() ,line_addr, cache_hit, useful_prefetch);

    for (int i = 0; i<prefetch_candidate.size(); i++) {
        uint64_t p_addr = prefetch_candidate[i] << LOG2_BLOCK_SIZE;
        if (p_addr == 0) break;
        champsim::address prefetch_addr{p_addr};
        prefetch_line(prefetch_addr, true, 0);
        
    }
  }
  return metadata_in;
}

uint32_t croage::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  uint64_t line_addr = (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);            // Line addr
  //uint64_t line_evicted = (evicted_addr >> LOG2_BLOCK_SIZE); // Line addr

  // Add to the shadow cache
  //this->corres_cache_add(set, way, line_addr, prefetch);
  return metadata_in;
}

void croage::prefetcher_final_stats() {
  this->print_status();
  cout<<"Metadata_Miss: "<<no_predict<<endl;
  for (const auto &item : in_metadata){
    if (item.second == true) useful_entry_number++;
    else useless_entry_number++;
  }
  cout<<"Useful_Entry: "<<useful_entry_number<<endl;
  cout<<"Useless_Entry: "<<useless_entry_number<<endl;
 }

void croage::prefetcher_cycle_operate() {}