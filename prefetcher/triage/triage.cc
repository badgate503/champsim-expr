#include "triage.h"



TriageTrainingUnit::TriageTrainingUnit() {
    current_timer = 0;
}

void TriageTrainingUnit::set_conf(TriageConfig* conf) {
    max_size = conf->training_unit_size;
}

Metadata TriageTrainingUnit::set_addr(uint64_t pc, uint64_t addr) {
    auto it = entry_list.find(pc);
    Metadata result;
    if (it != entry_list.end()) {
        // pc exists already
        TriageTrainingUnitEntry &entry = it->second;
        entry.timer = current_timer++;
        int32_t delta = addr - entry.trigger_addr;
        uint64_t last_addr = entry.in_spatial ? entry.cur_spatial.last_addr : entry.trigger_addr;
        if (last_addr == addr) {
            // ignore repeated addresses
        } else if (entry.in_spatial) {
            // already in spatial
            if (entry.cur_spatial.matches(addr)) {
                // continuing existing spatial pattern
                entry.cur_spatial.add(addr);
            } else {
                // evicting old spatial pattern
                result.set_spatial(entry.trigger_addr, entry.cur_spatial);
                entry.in_spatial = false;
                entry.trigger_addr = addr;
            }
        } else if (COMPRESS_METADATA && 
                  delta >= -MAX_DELTA && delta < MAX_DELTA && 
                  (addr >> LOG2_REGION_SIZE == entry.trigger_addr >> LOG2_REGION_SIZE)) {
            // creating new spatial
            entry.in_spatial = true;
            entry.cur_spatial = DeltaPattern(delta, addr);
        } else {
            // just another temporal
            result.set_addr(entry.trigger_addr);
            entry.trigger_addr = addr;          // This is not fit the paper's description
        }
    } else {
        // this pc does not exist yet
        if (entry_list.size() == max_size) 
            evict();
        entry_list[pc].in_spatial = false;
        entry_list[pc].trigger_addr = addr;
        entry_list[pc].timer = current_timer++;
    }
    assert(entry_list.size() <= max_size);
    return result;
}

void TriageTrainingUnit::evict() {
    assert(entry_list.size() == max_size);
    map<uint64_t, TriageTrainingUnitEntry>::iterator it, min_it;
    uint64_t min_timer = current_timer;
    for (it = entry_list.begin(); it != entry_list.end(); it++) {
        assert(it->second.timer < current_timer);
        if (it->second.timer < min_timer) {
            min_it = it;
            min_timer = it->second.timer;
        }
    }

    entry_list.erase(min_it);
}

TriageRepl* TriageRepl::create_repl(std::vector<std::map<uint64_t, TriageOnchipEntry> >* entry_list, uint32_t assoc){
    if (!entry_list) {
        throw std::runtime_error("Null entry_list in create_repl");
    }
    
    std::cout << "Creating repl with entry_list size: " << entry_list->size() << std::endl;
    auto repl = new TriageReplHawkeye(entry_list, assoc);
    std::cout << "Returning repl: " << repl << " with entry_list: " << entry_list << std::endl;
    return repl;
}

// void TriageOnchip::set_conf(TriageConfig *config) {
//     assoc = ON_CHIP_ASSOC;
//     num_sets = ON_CHIP_SET;
//     index_mask = num_sets - 1;

//     entry_list.resize(num_sets);
//     repl = TriageRepl::create_repl(&entry_list, assoc);
// }

// uint64_t TriageOnchip::get_line_offset(uint64_t addr) {
//     uint64_t line_offset = addr & 0x3f;
//     return line_offset;
// }


uint64_t TriageOnchip::get_set_id(uint64_t addr) {
    uint64_t set_id = ((addr >> 6) ^ (addr >> 12)) & index_mask;
    assert(set_id < num_sets);
    return set_id;
}

uint64_t TriageOnchip::get_line_offset(uint64_t addr) {
    return addr & 0x3f;
}

int TriageOnchip::increase_confidence(uint64_t addr) {
    uint64_t set_id = get_set_id(addr);
    assert(set_id < num_sets);
    uint64_t tag = addr;
    map<uint64_t, TriageOnchipEntry>& entry_map = entry_list[set_id];
    map<uint64_t, TriageOnchipEntry>::iterator it = entry_map.find(tag);
    if (it == entry_map.end()) {
        // 处理未找到的情况，可以返回一个默认值
        return 0; 
    }

    it->second.increase_confidence();
    return it->second.confidence;
}

int TriageOnchip::decrease_confidence(uint64_t addr) {
    uint64_t set_id = get_set_id(addr);
    assert(set_id < num_sets);
    uint64_t tag = addr;
    map<uint64_t, TriageOnchipEntry>& entry_map = entry_list[set_id];
    map<uint64_t, TriageOnchipEntry>::iterator it = entry_map.find(tag);
    if (it == entry_map.end()) {
        // 处理未找到的情况，可以返回一个默认值
        return 0; 
    }

    it->second.decrease_confidence();
    return it->second.confidence;
}

void TriageOnchip::update(uint64_t prev_addr, Metadata next_entry, uint64_t pc, bool update_repl, map<uint64_t, bool>& in_metadata, uint64_t& useful_entry_number, uint64_t& useless_entry_number) {
    uint64_t set_id = get_set_id(prev_addr);
    assert(set_id < num_sets);
    uint64_t tag = prev_addr;
    map<uint64_t, TriageOnchipEntry>& entry_map = entry_list[set_id];
    map<uint64_t, TriageOnchipEntry>::iterator it = entry_map.find(tag);
    if (it != entry_map.end()) {
        while (entry_map.size() > assoc && entry_map.size() > 0) {
            uint64_t victim_addr = repl->pickVictim(set_id);
            assert(entry_map.count(victim_addr));
            entry_map.erase(victim_addr);
            
            
            if (in_metadata[victim_addr] == true) useful_entry_number++;
            else useless_entry_number++;
            in_metadata.erase(victim_addr);
            
        }
        if(update_repl)
            repl->addEntry(set_id, tag, pc);
    } else {
        while (entry_map.size() >= assoc && entry_map.size() > 0) {
            uint64_t victim_addr = repl->pickVictim(set_id);
            assert(entry_map.count(victim_addr));
            entry_map.erase(victim_addr);

            if (in_metadata[victim_addr] == true) useful_entry_number++;
            else useless_entry_number++;
            in_metadata.erase(victim_addr);
        }
        
        assert(!entry_map.count(tag));

        entry_map[tag] = TriageOnchipEntry();
        entry_map[tag].metadata = next_entry;
        entry_map[tag].confidence = 1;
        entry_map[tag].valid = true;
        repl->addEntry(set_id, tag, pc);
    }
    assert(entry_map.size() <= assoc);
}

Metadata TriageOnchip::get_next_entry(uint64_t prev_addr, uint64_t pc, bool update_stats) {
    uint64_t set_id = get_set_id(prev_addr);
    assert(set_id < num_sets);
    uint64_t tag = prev_addr;
    map<uint64_t, TriageOnchipEntry>& entry_map = entry_list[set_id];
    
    map<uint64_t, TriageOnchipEntry>::iterator it = entry_map.find(tag);

    Metadata next_entry;
    if (it != entry_map.end() && (it->second.valid)) {
        next_entry = it->second.metadata;
        if (update_stats) {
            repl->addEntry(set_id, tag, pc);
        }
    }
    return next_entry;
}

uint32_t TriageOnchip::get_assoc()
{
    return assoc;
}

void TriageOnchip::print_stats()
{
    repl->print_stats();
}

// void Triage::test() {
//     train(0, 0, 0);
//     train(0, 1, 0);
//     train(0, 4, 0);

//     Metadata c1 = on_chip_data.get_next_entry(0, 0, false);
//     assert(c1.spatial);
//     assert(c1.next_spatial.predict(0).size() == 1);
//     assert(c1.next_spatial.predict(0)[0] == 1);
// }

// Triage::Triage() {
//     trigger_count = 0;
//     predict_count = 0;
//     same_addr = 0;
//     new_addr = 0;
//     no_next_addr = 0;
//     conf_dec_retain = 0;
//     conf_dec_update = 0;
//     conf_inc = 0;
//     new_stream = 0;
//     total_assoc = 0;
//     spatial = 0;
//     temporal = 0;
// }

void triage::set_conf(TriageConfig *config) {
    lookahead = config->lookahead;
    degree = config->degree;

    training_unit.set_conf(config);
    on_chip_data.set_conf(config);
}

void triage::train(uint64_t pc, uint64_t addr, bool cache_hit) {
    if (cache_hit) {
        training_unit.set_addr(pc, addr);
        return;
    }
    Metadata new_entry = training_unit.set_addr(pc, addr);
    if (new_entry.valid) {
        
        in_metadata[addr] = false;
            
        bool is_spatial = new_entry.spatial;
        if (!is_spatial && new_entry.addr == addr) {
            // Same Addr
            same_addr++;
        } else {
            // New Addr
            new_addr++;

            uint64_t trigger_addr;
            if (is_spatial) {
                spatial += new_entry.next_spatial.size();
                // if spatial, new_entry contains the real trigger addr
                trigger_addr = new_entry.addr;
            } else {
                temporal++;
                // if temporal, correlate old address with new one
                trigger_addr = new_entry.addr;
                new_entry.addr = addr;
            }
            
            Metadata next_entry = on_chip_data.get_next_entry(trigger_addr, pc, false);
            if (!next_entry.valid) {
                // no valid correlation for trigger_addr yet
                on_chip_data.update(trigger_addr, new_entry, pc, true, in_metadata, useful_entry_number, useless_entry_number);
                no_next_addr++;
            } else if (next_entry != new_entry) {
                // existing correlation doesn't match the new one
                int conf = on_chip_data.decrease_confidence(trigger_addr);
                conf_dec_retain++;
                if (conf == 0) {
                    conf_dec_update++;
                    on_chip_data.update(trigger_addr, new_entry, pc, false, in_metadata, useful_entry_number, useless_entry_number);
                }
            } else {
                // existing correlation matches this one
                on_chip_data.increase_confidence(trigger_addr);
                conf_inc++;
            }

            if (new_entry.spatial) {
                // create a link to the next address, if necessary
                trigger_addr = new_entry.next_spatial.last_addr;
                Metadata link_entry;
                link_entry.set_addr(addr);

                next_entry = on_chip_data.get_next_entry(trigger_addr, pc, false);
                if (!next_entry.valid) {
                    // no valid correlation for trigger_addr yet
                    on_chip_data.update(trigger_addr, link_entry, pc, true, in_metadata, useful_entry_number, useless_entry_number);
                    no_next_addr++;
                } else if (next_entry != link_entry) {
                    // existing correlation doesn't match the new one
                    int conf = on_chip_data.decrease_confidence(trigger_addr);
                    conf_dec_retain++;
                    if (conf == 0) {
                        conf_dec_update++;
                        on_chip_data.update(trigger_addr, link_entry, pc, false, in_metadata, useful_entry_number, useless_entry_number);
                    }
                } else {
                    // existing correlation matches this one
                    on_chip_data.increase_confidence(trigger_addr);
                    conf_inc++;
                }
            }
        }
    } else {
        new_stream++;
    }
}

void triage::predict(uint64_t pc, uint64_t addr, bool cache_hit) {
    Metadata next_entry = on_chip_data.get_next_entry(addr, pc, false);
    if (next_entry.valid) {
        
        if (next_entry.spatial) {
                predict_count++;
            for (uint64_t pred : next_entry.next_spatial.predict(addr)) {
                
                next_addr_list.push_back(pred);
                assert(pred != addr);
            }
        } else {
            uint64_t next_addr = next_entry.addr;
            predict_count++;
            
            next_addr_list.push_back(next_addr);
            assert(next_addr != addr);
        }
    }
    
}

void triage::calculatePrefetch(uint64_t pc, uint64_t addr, bool cache_hit, uint64_t *prefetch_list, int max_degree) {
    // XXX Only allow lookahead = 1 and degree=1 for now
    assert(lookahead == 1);
    assert(degree == 1);

    assert(degree <= max_degree);
    
    if (pc == 0) return; //TODO: think on how to handle prefetches from lower level

    next_addr_list.clear();
    trigger_count++;
    total_assoc += get_assoc();

    // Predict
    predict(pc, addr, cache_hit);

    // Train
    train(pc, addr, cache_hit);

    for (size_t i = 0; i < max_degree && i<next_addr_list.size(); i++){
        prefetch_list[i] = next_addr_list[i]<< LOG2_BLOCK_SIZE;
        metadata_hit++;
    }
        
}

uint32_t triage::get_assoc() {
    return on_chip_data.get_assoc();
}

void triage::print_stats() {
    cout << dec << "trigger_count=" << trigger_count <<endl;
    cout << "predict_count=" << predict_count <<endl;
    cout << "same_addr=" << same_addr <<endl;
    cout << "new_addr=" << new_addr <<endl;
    cout << "new_stream=" << new_stream <<endl;
    cout << "no_next_addr=" << no_next_addr <<endl;
    cout << "conf_dec_retain=" << conf_dec_retain <<endl;
    cout << "conf_dec_update=" << conf_dec_update <<endl;
    cout << "conf_inc=" << conf_inc <<endl;
    cout << "total_assoc=" << total_assoc <<endl;
    cout << "spatial=" << spatial << endl;
    cout << "temporal=" << temporal << endl;

    on_chip_data.print_stats();
}


//16K entries = 64KB
// void CACHE::llc_prefetcher_initialize()
// {
//     for(uint32_t cpu=0; cpu < NUM_CPUS; cpu++) {
//     conf[cpu].lookahead = 1;
//     conf[cpu].degree = 1;
    
//     conf[cpu].on_chip_set = ON_CHIP_SET;
//     conf[cpu].on_chip_assoc = ON_CHIP_ASSOC;
// //    conf[cpu].on_chip_assoc = 524288;
//     conf[cpu].training_unit_size = 128;

//     triage[cpu].set_conf(&conf[cpu]);
//     }
// }

uint32_t triage::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
    uint32_t metadata_in)
{
    //cpu = 0;
    if (type != access_type::LOAD) {
        return metadata_in;
    }
    

    

    //if (cache_hit) {
    //    return metadata_in;
    //}
    uint64_t line_addr = (addr.to<uint64_t>() >> LOG2_BLOCK_SIZE);
    
    if (line_addr == this->last_address)
        return metadata_in;
    this->last_address = line_addr;

    int i;
    uint64_t prefetch_addr_list[MAX_ALLOWED_DEGREE];
    for (i = 0; i < MAX_ALLOWED_DEGREE; ++i) {
        prefetch_addr_list[i] = 0;
    }
    this->calculatePrefetch(ip.to<uint64_t>(), line_addr, cache_hit, prefetch_addr_list,
            MAX_ALLOWED_DEGREE);
    
    if (prefetch_addr_list[0]==0) no_predict++;
    else{
        
        in_metadata[line_addr] = true;
    } 

    int prefetched = 0;
    for (i = 0; i < MAX_ALLOWED_DEGREE; ++i) {
        if (prefetch_addr_list[i] == 0) {
            break;
        }
        
        champsim::address prefetch_addr{prefetch_addr_list[i]};
        prefetch_line(prefetch_addr, true, 0);
        
    }
    // Set cache assoc if dynamic
//    if (conf[cpu].use_dynamic_assoc) {
//        cout << "LLC WAY: " << LLC_WAY << ", ASSOC: " << data[cpu].get_assoc() << endl;
//        current_assoc = LLC_WAY - data[cpu].get_assoc();
//    }
    
    return metadata_in;
}

uint32_t triage::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
    //cpu = 0;
    // if(prefetch) {
    //     uint64_t next_addr;
    //     bool next_addr_exists = triage[cpu].on_chip_data.get_next_addr(metadata_in, next_addr, 0, true);
    //     //assert(next_addr_exists);
    //     //cout << "Filled " << hex << addr << "  by " << metadata_in << endl;
    // }
    return metadata_in;
}

//void triage::prefetcher_cycle_operate() {}

