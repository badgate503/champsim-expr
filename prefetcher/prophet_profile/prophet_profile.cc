#include "prophet_profile.h"

#include <utility>
#include <cassert>




void prophet_profile::invoke_prefetcher(uint64_t ip, uint64_t addr, uint8_t cache_hit, uint8_t type, vector<uint64_t> &pref_addr)
{
    if (disablePF)
        return;
    if (enableDRA)
        return;
    if (ip == 0)
    {
        // DPRINTF(HWPrefetch, "Ignoring request with no PC.\n");
        return;
    }

    uint64_t block_addr = addr >> LOG2_BLOCK_SIZE;
    if (!cache_hit || prefetched_addr.find(addr >> LOG2_BLOCK_SIZE) != prefetched_addr.end())
    {
        allMisses += 1;
        if (missTable.find(ip) == missTable.end())
            missTable[ip] = 1;
        else
            missTable[ip] += 1;
    }

    if (trainTable.find(ip) == trainTable.end())
    {
        if (trainTable.size() < 128)
        {
            trainTable[ip] = TrainEntry();
        }
        else
        {
            std::map<uint64_t, TrainEntry>::iterator minPointer = trainTable.begin();
            for (std::map<uint64_t, TrainEntry>::iterator it = trainTable.begin(); it != trainTable.end(); ++it)
            {
                if (it->second.solved < minPointer->second.solved)
                {
                    minPointer = it;
                }
            }
            if (!minPointer->second.protect)
            {
                trainTable.erase(minPointer);
                trainTable[ip] = TrainEntry();
            }
            else
            {
                minPointer->second.protect = false;
            }
        }
    }
    else
    {
        if (cache_hit && prefetched_addr.find(block_addr) != prefetched_addr.end()){
            trainTable[prefetched_addr[block_addr]].solved += 1;
            prefetched_addr.erase(block_addr);
        }
    }

    if (enableInsertFilter && !inTraining && profileInsertTable.find(ip) == profileInsertTable.end())
        return;

    global_timestamp++;

    // 1.search
    // MetaEntry *metadata = &(metaTable->find(block_addr)->data);
    ProphetMetaTableEntry *metadata = metaTable->find(block_addr);
    ProphetMRBTableEntry *reuseData = mrbTable->find(block_addr);

    uint64_t lastAddr = pcTable.find(ip) != pcTable.end() ? pcTable[ip] : 0;
    if (lastAddr == block_addr)
        return;
    if (reuseData)
    {
        if (reuseData->counter < MRB_MAX_COUNTER)
            reuseData->counter++;
        mrbTable->set_mru(block_addr);
    }
    if (metadata)
    {
        metaTable->set_mru(block_addr);
        if (profileUtiTable.find(block_addr) != profileUtiTable.end())
        {
            profileUtiTable[block_addr]++;
        }
        else
        {
            profileUtiTable[block_addr] = 0;
        }
        if (!metadata->used)
        {
            metadata->used = true;
            // trainTable[metadata->pc].meta_used++;
        }
    }

    // 2.issue: metadata table
    uint64_t lookup = block_addr;
    int issued_by_metatable = issue_metatable(metaTable, lookup, ip, pref_addr);
    if (enableMRB)
    {
        int issued_by_reuse = issue_mrbtable(mrbTable, lookup, ip, pref_addr);
    }
    for (auto &prefetch_address : pref_addr)
    {
        // parent->prefetch_line(ip, addr, prefetch_address, FILL_L2, 0);
        prefetched_addr[prefetch_address >> LOG2_BLOCK_SIZE] = ip;
        trainTable[ip].issued += 1;
    }
    
    // 3.update
    // 3.1 update the metaTable

    if (lastAddr != 0)
    {
        ProphetMetaTableEntry *lastMeta = metaTable->find(lastAddr);
        if (lastMeta)
        {
            bool matched = false;
            if (lastMeta->correlatedAddr == block_addr)
            {
                matched = true;
            }
            if (!matched)
            {
                uint64_t victimAddr = lastMeta->correlatedAddr;
                //lastMeta->correlatedAddr = block_addr;   // change silently
                ProphetMetaTableEntry temp_entry(block_addr);
                metaTable->insert(lastAddr, temp_entry, 1);

                
                // std::ostringstream oss1;
                // oss1<<std::dec<<parent->current_cycle()<< " ADD "<<std::hex<<lastAddr<<" "<<block_addr;
                // std::ostringstream oss2;
                // oss2<<std::dec<<parent->current_cycle()<< " EVICT CONFLICT "<<std::hex<<lastAddr<<" "<<victimAddr;
                // logs.push_back(oss1.str());
                // logs.push_back(oss2.str());


                // victim buffer logic
                if (profileReplTable[ip] > 1)
                {
                    ProphetMRBTableEntry *victimMeta = mrbTable->find(lastAddr);
                    if (!victimMeta)
                    {
                        ProphetMRBTableEntry temp_entry(victimAddr);
                        mrbTable->insert(lastAddr, temp_entry);
                    }
                    else
                    {
                        if (victimMeta->correlatedAddr == victimAddr){
                            if (victimMeta->counter < MRB_MAX_COUNTER){
                                victimMeta->counter++;
                            }
                        }else{

                            ProphetMRBTableEntry temp_entry(victimAddr);
                            mrbTable->insert(lastAddr, temp_entry);
                            //victimMeta->counter = 0;
                        }
                    }
                }
            }
        }
        else
        {
            ProphetMetaTableEntry temp_entry(block_addr);
            if (!inTraining && enablePGLRU && profileReplTable.find(ip) != profileReplTable.end())
                metaTable->insert(lastAddr,temp_entry, profileReplTable[ip]);
            else{
                if (!metaTable->insert(lastAddr,temp_entry, 1)) // lyq: profile中，prio = 0 ？
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

int prophet_profile::issue_metatable(ProphetMetaTable *metaTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t> &addresses)
{
    int issued = 0;
    for (int i = 0; i < globalDegree; i++)
    {
        ProphetMetaTableEntry *candidate = metaTable->find(lookup);
        if (candidate == nullptr)
            break;
        addToUsedPool(lookup);
        if (candidate->correlatedAddr != 0){
            
            if (!isAlreadyInQueue(addresses, candidate->correlatedAddr << LOG2_BLOCK_SIZE))
            {
                addresses.push_back(candidate->correlatedAddr << LOG2_BLOCK_SIZE);
                std::ostringstream oss;         
                oss<<std::dec<<parent->current_cycle()<< " ISSUE MT "<< std::hex<<pc<<" "<< (lookup)<<" " << (candidate->correlatedAddr);
                logs.push_back(oss.str());    
                issued++;
            }
            lookup = candidate->correlatedAddr;
        }
    }
    return issued;
}

int prophet_profile::issue_mrbtable(ProphetMRBTable *mrbTable, uint64_t lookup, uint64_t pc, std::vector<uint64_t> &addresses)
{
    std::cout <<"deprecated,Should not issue" <<std::endl;
    int issued = 0;
    for (int i = 0; i < globalDegree; i++)
    {
        ProphetMRBTableEntry *candidate = mrbTable->find(lookup);
        if (candidate == nullptr)
            break;
        addToUsedPool(lookup);
        if (candidate->correlatedAddr != 0){
            lookup = candidate->correlatedAddr;
            if (!isAlreadyInQueue(addresses, candidate->correlatedAddr << LOG2_BLOCK_SIZE))
            {
                std::ostringstream oss;         
                oss<<std::dec<<parent->current_cycle()<< " ISSUE MRB "<< std::hex<<pc<<" "<< (lookup)<<" " << (candidate->correlatedAddr);
                logs.push_back(oss.str());    
                addresses.push_back(candidate->correlatedAddr << LOG2_BLOCK_SIZE);
                issued++;
            }
        }
    }
    return issued;
}

void prophet_profile::outPrefetcherPGOInfo(){
    
}


uint32_t prophet_profile::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in, std::string latepf )
{
    
    if(!cache_hit) { // L2 CACHE MISS
        if(ip.to<uint64_t>() != 0) {
            champsim::block_number pf_addr{addr};
            //std::ofstream ofs(out_file, std::ios::app);  // append mode
            uint64_t last_addr = get_last(ip.to<uint64_t>());
            std::set<uint64_t> triggers = get_triggers(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
            
            std::ostringstream oss;         
            oss<<std::dec<<parent->current_cycle()<< " MISS "<<latepf<<" "<<std::hex<<pf_addr<<" "<<ip<<" "<<last_addr;

            for(uint64_t t:triggers) {
                oss << " " << t;
            }
            logs.push_back(oss.str());

            //ofs.close();
        }
    } else {
         if(ip.to<uint64_t>() != 0) {
            champsim::block_number pf_addr{addr};
            //std::ofstream ofs(out_file, std::ios::app);  // append mode
            uint64_t last_addr = get_last(ip.to<uint64_t>());
            std::set<uint64_t> triggers = get_triggers(addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
            
            std::ostringstream oss;         
            oss<<std::dec<<parent->current_cycle()<< " HIT "<<std::hex<<pf_addr<<" "<<ip<<" "<<last_addr;

            for(uint64_t t:triggers) {
                oss << " " << t;
            }
            logs.push_back(oss.str());

            //ofs.close();
        }
    }


    vector<uint64_t> prefetch_candidate;

    invoke_prefetcher(ip.to<uint64_t>(), addr.to<uint64_t>(), cache_hit, 0, prefetch_candidate);
    
    if (prefetch_candidate.size()==0) return metadata_in;

    for (int i = 0; i<prefetch_candidate.size(); i++) {
      uint64_t p_addr = prefetch_candidate[i];
      
      if (p_addr == 0) break;
      champsim::address prefetch_addr{p_addr};
      const bool success = prefetch_line(prefetch_addr, true, 0);
    }
  return metadata_in;
}

uint32_t prophet_profile::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void prophet_profile::prefetcher_final_stats() {
    std::ofstream ofs(out_file);
    std::cout <<"hi!"<<logs.size()<<std::endl;
    for(std::string s : logs) {
        ofs << s << "\n";
    }
    ofs.close();
 }

void prophet_profile::prefetcher_cycle_operate() {}

bool ProphetMetaTable::insert(uint64_t key, const ProphetMetaTableEntry &data, uint8_t priority)
{
    reverse_metatable[data.correlatedAddr].insert(key);

    
    Entry victim_entry = Super::insert(key, data);
    Super::set_mru(key);
    uint64_t index = key % this->num_sets;
    uint64_t tag = key / this->num_sets;
    int way = this->cams[index][tag];
    priority_pgo[index][way] = priority;
    bool ret = false;
    if(victim_entry.valid) {
        std::ostringstream oss1;
        std::string reason;
        //std::cout << "hola" << std::endl;
        reverse_metatable[victim_entry.data.correlatedAddr].erase(victim_entry.key);
        
        if (victim_entry.tag != tag) {
            reason = "CAPACITY";
        } else {
            reason = "CONFLICT";
        }
        oss1<<std::dec<<pp->parent->current_cycle()<< " EVICT "<<reason<<" "<<std::hex<<victim_entry.key<<" "<<victim_entry.data.correlatedAddr;
        pp->logs.push_back(oss1.str());
        
        ret = true;
    }
    std::ostringstream oss;
    oss<<std::dec<<pp->parent->current_cycle()<< " ADD "<<std::hex<<key<<" "<<data.correlatedAddr;
    //pp->logs.push_back(oss.str());
    pp->logs.push_back(oss.str());
    
        
    return ret;
}

// } // namespace prefetch
// } // namespace gem5


