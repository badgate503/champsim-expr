#ifndef TRIAGE_H__
#define TRIAGE_H__

#pragma GCC optimize ("O2")

#include <stdint.h>
#include <assert.h>
#include <map>
#include <math.h>
#include <set>
#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <sys/types.h>

#include "address.h"
#include "champsim.h"
#include "modules.h"
#include "cache.h"

using namespace std;

#define COMPRESS_METADATA true
#define MAX_DELTA 64
#define MAX_DELTA_LENGTH 64
#define LOG2_REGION_SIZE 6

#define MAX_ALLOWED_DEGREE 4
#define ON_CHIP_SET 4096
#define ON_CHIP_ASSOC 32

/* interface for spatial patterns;
 * only one instance of this class should exist per binary */
struct SpatialPattern {
    /* determines if a new miss address continues the pattern */
    virtual bool matches(uint64_t) = 0;
    /* adds a new miss address to the pattern */
    virtual void add(uint64_t) = 0;
    /* returns how many addresses are represented by this pattern */
    virtual uint32_t size() = 0;
    /* returns the last address seen by the pattern */
    virtual uint64_t last_address() = 0;
    virtual bool operator==(const SpatialPattern&) const = 0;
    /* predicts a list of prefetches based on a trigger */
    virtual vector<uint64_t> predict(uint64_t) = 0;
    /* creates a clone of the spatial pattern */
    virtual SpatialPattern *clone() const = 0;
    virtual ~SpatialPattern() {};
};

/* a spatial pattern defined by a delta and a length
 * "simple" deltas only */
struct DeltaPattern : public SpatialPattern {
    int32_t delta;
    uint32_t length;
    uint64_t last_addr;

    DeltaPattern(int32_t d, uint64_t l) :
        delta(d), length(1), last_addr(l) {}

    DeltaPattern() :
        delta(0), length(0), last_addr(0) {}

    // Copy constructor
    DeltaPattern(const DeltaPattern& other) 
        : delta(other.delta), length(other.length), last_addr(other.last_addr) {}
    
    // Move constructor
    DeltaPattern(DeltaPattern&& other) noexcept
        : delta(other.delta), length(other.length), last_addr(other.last_addr) {
        other.delta = 0;
        other.length = 0;
        other.last_addr = 0;
    }
    
    // Copy assignment
    DeltaPattern& operator=(const DeltaPattern& other) {
        if (this != &other) {
            delta = other.delta;
            length = other.length;
            last_addr = other.last_addr;
        }
        return *this;
    }
    
    // Move assignment
    DeltaPattern& operator=(DeltaPattern&& other) noexcept {
        if (this != &other) {
            delta = other.delta;
            length = other.length;
            last_addr = other.last_addr;
            
            other.delta = 0;  
            other.length = 0;
            other.last_addr = 0;
        }
        return *this;
    }

    ~DeltaPattern() {}

    virtual bool matches(uint64_t addr_B) override {
        int32_t last_delta = addr_B - last_addr;
        return (last_delta % delta == 0) && 
                length < MAX_DELTA_LENGTH &&
                (addr_B >> LOG2_REGION_SIZE) == (last_addr >> LOG2_REGION_SIZE);
    }

    virtual void add(uint64_t addr_B) override {
        length++;
    }

    virtual uint32_t size() override { return length; }

    virtual uint64_t last_address() override { return last_addr; }

    virtual bool operator==(const SpatialPattern &other) const override {
        /* casting allowed here because only one type of SpatialPattern
         * can be instantiated in a binary */ 
        DeltaPattern other_delta = *((DeltaPattern*) ((void*) &other));
        return delta == other_delta.delta && length == other_delta.length;
    }

    virtual vector<uint64_t> predict(uint64_t trigger) override {
        vector<uint64_t> result;
        for (int32_t i = 1; i <= (int32_t) length; i++) {
            result.push_back(trigger+delta*i);
        }
        return result;
    }

    virtual SpatialPattern *clone() const override {
        DeltaPattern *result = new DeltaPattern();
        result->delta = delta;
        result->length = length;
        result->last_addr = last_addr;
        return result;
    }

    friend ostream& operator <<(ostream &os, const DeltaPattern &other) {
        os << "delta " << other.delta << " with length " << other.length;
        return os;
    }
};

struct TriageConfig {
    int lookahead = 1;
    int degree = 1;

    int on_chip_set = ON_CHIP_SET, on_chip_assoc = ON_CHIP_ASSOC;
    int training_unit_size = 128;
};

struct Metadata {
    bool valid;
    bool spatial;
    DeltaPattern next_spatial;
    uint64_t addr;

    // Default constructor
    Metadata() : valid(false), spatial(false), next_spatial(), addr(0) {}

    // Copy constructor
    Metadata(const Metadata& other)
        : valid(other.valid)
        , spatial(other.spatial)
        , next_spatial(other.next_spatial)
        , addr(other.addr) {}

    // Move constructor
    Metadata(Metadata&& other) noexcept
        : valid(other.valid)
        , spatial(other.spatial)
        , next_spatial(std::move(other.next_spatial))
        , addr(other.addr) {
        other.valid = false;
        other.spatial = false;
        other.addr = 0;
    }

    // Copy assignment
    Metadata& operator=(const Metadata& other) {
        if (this != &other) {
            valid = other.valid;
            spatial = other.spatial;
            next_spatial = other.next_spatial;
            addr = other.addr;
        }
        return *this;
    }

    // Move assignment
    Metadata& operator=(Metadata&& other) noexcept {
        if (this != &other) {
            valid = other.valid;
            spatial = other.spatial;
            next_spatial = std::move(other.next_spatial);
            addr = other.addr;
            
            other.valid = false;
            other.spatial = false;
            other.addr = 0;
        }
        return *this;
    }
    

    void set_addr(uint64_t next_addr) {
        valid = true;
        spatial = false;
        addr = next_addr;
    }

    void set_spatial(uint64_t trigger, DeltaPattern dp) {
        valid = true;
        addr = trigger;
        spatial = true;
        next_spatial = dp;
    }
    
    bool operator==(const Metadata& other) const {
        return (spatial && other.spatial && next_spatial == other.next_spatial)
                || (!spatial && !other.spatial && addr == other.addr);
    }

    bool operator!=(const Metadata& other) const { return !(*this == other); }
};

struct TriageTrainingUnitEntry {
    // are we in the middle of training a spatial pattern?
    bool in_spatial;
    // stores the trigger address for a spatial pattern, 
    // or simply the last address
    uint64_t trigger_addr;
    // stores the current spatial pattern
    DeltaPattern cur_spatial;
    // used for LRU purposes
    uint64_t timer;
};

class TriageTrainingUnit {
    // XXX Only support fully associative LRU for now
    // PC->TrainingUnitEntry
    std::map<uint64_t, TriageTrainingUnitEntry> entry_list;
    uint64_t current_timer;
    uint64_t max_size;

    void evict();

    public:
        TriageTrainingUnit();
        void set_conf(TriageConfig* conf);
        Metadata set_addr(uint64_t pc, uint64_t addr);
};



struct ADDR_INFO
{
    uint64_t last_quanta;
    uint64_t PC; 
    // bool last_prediction;
    // bool is_high_cost_predicted; // for Obol

    void init(uint64_t curr_quanta)
    {
        last_quanta = curr_quanta;
        PC = 0;
        
    }

    void update(uint64_t curr_quanta, uint64_t _pc)
    {
        last_quanta = curr_quanta;
        PC = _pc;
        // last_prediction = prediction;
        // is_high_cost_predicted = is_next_high_cost;
    }
};

#define LIVENESS_LENGTH 1024
struct OPTgen
{
    vector<unsigned int> liveness_history;

    uint64_t num_cache;
    uint64_t num_dont_cache;
    uint64_t access;
    uint64_t prefetch;
    uint64_t prefetch_cachehit;
    uint64_t assoc;

    void init(uint64_t size)
    {
        num_cache = 0;
        num_dont_cache = 0;
        access = 0;
        assoc = size;
        prefetch = 0;
        prefetch_cachehit = 0;
        liveness_history.resize(LIVENESS_LENGTH);
    }

    void add_access(uint64_t curr_quanta)
    {
        access++;
        liveness_history[curr_quanta%LIVENESS_LENGTH] = 0;
        // liveness_history.resize(curr_quanta+1);
        
        // TODO: initialize liveness with 0 or 1?
    }

    bool should_cache(uint64_t curr_quanta, uint64_t last_quanta, bool prefetch=false)
    {
        if (curr_quanta - last_quanta>1023) return false;
        bool is_cache = true;
        curr_quanta = curr_quanta % LIVENESS_LENGTH;
        last_quanta = last_quanta % LIVENESS_LENGTH;

        // assert(curr_quanta <= liveness_history.size());
        unsigned int i = last_quanta;
        while (i < curr_quanta)
        {
            if(liveness_history[i] >= assoc)
            {
                is_cache = false;
                break;
            }
            ++i;
            
            
        }

        //if ((is_cache) && (last_quanta != curr_quanta))
        if (is_cache)
        {
            i = last_quanta;
            while (i < curr_quanta)
            {
                liveness_history[i]++;
                //if (i>LIVENESS_LENGTH-1) i%=LIVENESS_LENGTH;
                ++i;
            }
            //assert(i == curr_quanta);
        }

        // if(!prefetch)
        // {
        //     if (is_cache) num_cache++;
        //     else num_dont_cache++;
        // }
        // else
        // {
        //     if(is_cache)
        //         prefetch_cachehit++;
        // }

        return is_cache;    
    }

    // uint64_t get_traffic()
    // {
    //     return (prefetch - prefetch_cachehit + access - num_cache);
    // }

};

class HawkeyePCPredictor
{
    map<uint64_t, short unsigned int > SHCT;
    const int MAX_SHCT = 8191;
    const int SHCT_SIZE_BITS = 14;
    const uint32_t SHCT_SIZE = 1<<SHCT_SIZE_BITS;

    inline uint64_t MyCRC( uint64_t _blockAddress)
    {
        static const unsigned long long crcPolynomial = 3988292384ULL;
        unsigned long long _returnVal = _blockAddress;
        for( unsigned int i = 0; i < 32; i++ )
            _returnVal = ( ( _returnVal & 1 ) == 1 ) ? ( ( _returnVal >> 1 ) ^ crcPolynomial ) : ( _returnVal >> 1 );
        return _returnVal;
    }

    public:
    void increment(uint64_t pc)
    {
        uint64_t signature = MyCRC(pc) % SHCT_SIZE;
        if(SHCT.find(signature) == SHCT.end())
            SHCT[signature] = (1+MAX_SHCT)/2;

        SHCT[signature] = (SHCT[signature] < MAX_SHCT) ? (SHCT[signature]+1) : MAX_SHCT;

    }

    void saturate(uint64_t pc)
    {
        uint64_t signature = MyCRC(pc) % SHCT_SIZE;
        assert(SHCT.find(signature) != SHCT.end());

        SHCT[signature] = MAX_SHCT;

    }

    void decrement (uint64_t pc)
    {
        uint64_t signature = MyCRC(pc) % SHCT_SIZE;
        if(SHCT.find(signature) == SHCT.end())
            SHCT[signature] = (1+MAX_SHCT)/2;
        if(SHCT[signature] != 0)
            SHCT[signature] = SHCT[signature]-1;
    }

    bool get_prediction (uint64_t pc)
    {
        uint64_t signature = MyCRC(pc) % SHCT_SIZE;
        if(SHCT.find(signature) != SHCT.end() && SHCT[signature] < ((MAX_SHCT+1)/2))
            return false;
        return true;
    }
};

struct TriageOnchipEntry {
    Metadata metadata;
    static const int denoise = 1;
    int confidence;
    int rrpv;
    bool valid;

    // Default constructor
    TriageOnchipEntry()
        : confidence(denoise)
        , rrpv(0)
        , valid(false) {
            metadata.spatial = false;
            metadata.addr = 0;
        }

    // Copy constructor
    TriageOnchipEntry(const TriageOnchipEntry& other)
        : metadata(other.metadata)
        , confidence(other.confidence)
        , rrpv(other.rrpv)
        , valid(other.valid) {}

    // Move constructor
    TriageOnchipEntry(TriageOnchipEntry&& other) noexcept
        : metadata(std::move(other.metadata))
        , confidence(other.confidence)
        , rrpv(other.rrpv)
        , valid(other.valid) {
        other.confidence = denoise;
        other.rrpv = 0;
        other.valid = false;
    }

    // Copy assignment
    TriageOnchipEntry& operator=(const TriageOnchipEntry& other) {
        if (this != &other) {
            metadata = other.metadata;
            confidence = other.confidence;
            rrpv = other.rrpv;
            valid = other.valid;
        }
        return *this;
    }

    // Move assignment
    TriageOnchipEntry& operator=(TriageOnchipEntry&& other) noexcept {
        if (this != &other) {
            metadata = std::move(other.metadata);
            confidence = other.confidence;
            rrpv = other.rrpv;
            valid = other.valid;
            
            other.confidence = denoise;
            other.rrpv = 0;
            other.valid = false;
        }
        return *this;
    }

    void increase_confidence(){
        if (confidence<denoise) confidence++;
    }
    void decrease_confidence(){
        if (confidence>0) confidence--;
    }
    void init(){
        metadata.spatial = false;
        metadata.addr = 0;
        confidence = denoise;
        valid = false;
        rrpv = 0;
    }
};

class TriageRepl {
    protected:
        std::vector<std::map<uint64_t, TriageOnchipEntry> > *entry_list = nullptr;

    public:
        TriageRepl(std::vector<std::map<uint64_t, TriageOnchipEntry> >* entry_list): entry_list(entry_list) {
            if (!entry_list) {
                throw std::runtime_error("Null entry_list");
            }
        }

        
        virtual void addEntry(uint64_t set_id, uint64_t addr, uint64_t pc) = 0;
        virtual uint64_t pickVictim(uint64_t set_id) = 0;
        virtual void print_stats() {}

        static TriageRepl* create_repl(std::vector<std::map<uint64_t, TriageOnchipEntry> >* entry_list, uint32_t assoc);

        // 禁用拷贝
        TriageRepl(const TriageRepl&) = delete;
        TriageRepl& operator=(const TriageRepl&) = delete;
        
        // 允许移动
        TriageRepl(TriageRepl&& other) noexcept : entry_list(other.entry_list) {
            other.entry_list = nullptr;
        }
        
        TriageRepl& operator=(TriageRepl&& other) noexcept {
            if (this != &other) {
                entry_list = other.entry_list;
                other.entry_list = nullptr;
            }
            return *this;
        }
            
        // Virtual destructor
        virtual ~TriageRepl() = default;
};

class TriageReplHawkeye : public TriageRepl
{
  unsigned max_rrpv = 7;
  std::vector<uint64_t> optgen_mytimer;
  // std::vector<OPTgen> optgen;
  std::vector<OPTgen> sample_optgen;
  std::map<uint64_t, ADDR_INFO> optgen_addr_history;
  std::map<uint64_t, uint64_t> signatures;
  HawkeyePCPredictor predictor;
  

  uint64_t bitmask(uint8_t l){
    return (((l) >= 64) ? (unsigned long long)(-1LL) : ((1LL << (l))-1LL));
  }

  uint64_t bits(uint64_t x,uint8_t i, uint8_t l) { return (((x) >> (i)) & bitmask(l));}

public:
    TriageReplHawkeye(std::vector<std::map<uint64_t, TriageOnchipEntry>>* entry_list, uint32_t assoc)
    : TriageRepl(entry_list)
    , max_rrpv(7)
    {
    if (!entry_list) {
        std::cerr << "Fatal: entry_list pointer is null" << std::endl;
        throw std::invalid_argument("entry_list is null");
    }

    uint64_t num_sets = entry_list->size();
    if (num_sets == 0 || num_sets > ON_CHIP_SET) {
        std::cerr << "Fatal: Invalid number of sets: " << num_sets << std::endl;
        throw std::range_error("Invalid number of sets");
    }

    std::cout << "TriageReplHawkeye: Initializing with " << num_sets << " sets" << std::endl;

    // 初始化向量
    optgen_mytimer = std::vector<uint64_t>(num_sets, 0);
    sample_optgen = std::vector<OPTgen>(num_sets);

    // 初始化每个 OPTgen
    for (size_t i = 0; i < num_sets; ++i) {
        sample_optgen[i].init(assoc);
    }

    std::cout << "TriageReplHawkeye: Initialization complete" << std::endl;
    }


    void addEntry(uint64_t set_id, uint64_t addr, uint64_t pc) override {
        if (!entry_list) {
            std::cerr << "Error: Null entry_list in addEntry" << std::endl;
            return;
        }

        if (set_id >= entry_list->size()) {
            std::cerr << "Error: set_id " << set_id << " out of bounds (" << entry_list->size() << ")" << std::endl;
            return;
        }

        if (set_id >= optgen_mytimer.size()) {
            std::cerr << "Error: set_id " << set_id << " exceeds optgen_mytimer size" << std::endl;
            return;
        }
        try {
        auto& entry_map = (*entry_list)[set_id];
        // 安全地进行map操作，避免直接使用迭代器
        
            uint64_t curr_quanta = optgen_mytimer[set_id];
            
            // 存储签名
            signatures[addr] = pc;
            
            // 更新 OPTgen
            if (optgen_addr_history.find(addr) != optgen_addr_history.end()) {
                auto& history = optgen_addr_history[addr];
                if (curr_quanta >= history.last_quanta) {
                    bool opt_hit = sample_optgen[set_id].should_cache(curr_quanta, history.last_quanta, false);
                    sample_optgen[set_id].add_access(curr_quanta);
                    
                    if (opt_hit) {
                        predictor.increment(history.PC);
                    } else {
                        predictor.decrement(history.PC);
                    }
                }
            } else {
                optgen_addr_history[addr].init(curr_quanta);
                sample_optgen[set_id].add_access(curr_quanta);
            }

            // 更新历史
            optgen_addr_history[addr].update(curr_quanta, pc);
            optgen_mytimer[set_id]++;

            // 更新替换策略状态
            bool prediction = predictor.get_prediction(pc);
            if (prediction) {
                size_t high_rrpv_count = 0;
                for (const auto& pair : entry_map) {
                    if (pair.second.rrpv >= max_rrpv - 1) {
                        high_rrpv_count++;
                    }
                }

                if (high_rrpv_count == 0) {
                    for (auto& pair : entry_map) {
                        pair.second.rrpv++;
                    }
                }

                auto it = entry_map.find(addr);
                if (it != entry_map.end()) {
                    it->second.rrpv = 0;
                }
            } else {
                auto it = entry_map.find(addr);
                if (it != entry_map.end()) {
                    it->second.rrpv = max_rrpv;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in addEntry: " << e.what() << std::endl;
        }
    }

  uint64_t pickVictim(uint64_t set_id){
    if (set_id >= optgen_mytimer.size()) {
        return 0;  // 返回安全值
    }
        map<uint64_t, TriageOnchipEntry>& entry_map = (*entry_list)[set_id];
        if (entry_map.empty()) {
            return 0;
        }
        try{
            for(auto it = entry_map.begin(); it != entry_map.end(); it++) {
                if (it->second.rrpv == max_rrpv) {
                    uint64_t addr = it->first;
                    return addr;
                }
            }
            //If we cannot find a cache-averse line, we evict the oldest cache-friendly line
            uint32_t max_rrip = 0;
            uint64_t lru_victim = 0;
            for (auto it = entry_map.begin(); it != entry_map.end(); ++it) {
                if (it->second.rrpv >= max_rrip)
                {
                    max_rrip = it->second.rrpv;
                    lru_victim = it->first;
                }
            }
            assert(entry_map.count(lru_victim));
    
            //The predictor is trained negatively on LRU evictions
            predictor.decrement(signatures[lru_victim]);
            
            return lru_victim;
        }
        catch (const std::exception& e) {
            std::cerr << "Error in pickVictim: " << e.what() << std::endl;
            return 0;
        }
        
    }

};



class TriageOnchip
{
  uint32_t num_sets, assoc;
  std::vector<std::map<uint64_t, TriageOnchipEntry>> entry_list;
  uint64_t index_mask;
  TriageRepl* repl;
  

  uint64_t get_set_id(uint64_t addr);
  uint64_t get_line_offset(uint64_t addr);

public:
  //TriageOnchip(){}
    // Default constructor
    TriageOnchip() {
        num_sets = ON_CHIP_SET;
        assoc = ON_CHIP_ASSOC;
        index_mask = num_sets - 1;

        // Resize entry_list and clear existing entries
        entry_list.clear();
        entry_list.resize(num_sets);

        std::cout << "TriageOnchip init: entry_list size = " << entry_list.size() << std::endl;
        std::cout << "TriageOnchip init: entry_list addr = " << &entry_list << std::endl;
        
        // 创建替换策略
        repl = TriageRepl::create_repl(&entry_list, assoc);
        if (!repl) {
            throw std::runtime_error("Failed to create replacement policy");
        }
        
        std::cout << "TriageOnchip: repl created at " << repl << std::endl;
        // // 确保 vector 正确初始化
        // entry_list.clear();
        // entry_list.resize(num_sets);
        
        // // 初始化每个 set 的 map
        // for (auto& set : entry_list) {
        //     set.clear();  // 确保 map 是空的
        // }
        
        // // 创建替换策略
        // repl = TriageRepl::create_repl(&entry_list, assoc);
        
        // if (!repl) {
        //     throw std::runtime_error("Failed to create replacement policy");
        // }
    }

    // 拷贝构造函数
    TriageOnchip(const TriageOnchip& other) 
        : num_sets(other.num_sets)
        , assoc(other.assoc)
        , index_mask(other.index_mask)
        , entry_list(other.entry_list)
        , repl(nullptr)
    {
        if (other.repl) {
            repl = TriageRepl::create_repl(&entry_list, assoc);
        }
        std::cout << "TriageOnchip: Copy constructor called" << std::endl;
    }
    
    // 移动构造函数
    TriageOnchip(TriageOnchip&& other) noexcept
        : num_sets(other.num_sets)
        , assoc(other.assoc)
        , index_mask(other.index_mask)
        , entry_list(std::move(other.entry_list))
        , repl(other.repl)
    {
        std::cout << "TriageOnchip move constructor called" << std::endl;
        std::cout << "After move, entry_list size = " << entry_list.size() << std::endl;
        std::cout << "After move, entry_list address = " << &entry_list << std::endl;
        std::cout << "After move, old repl = " << repl << std::endl;
        
        // 关键：创建新的 repl 指向新的 entry_list
        // 原来的 repl 指向的是 other.entry_list，现在需要更新
        other.repl = nullptr;  // 防止原对象析构时删除 repl
        
        // 删除旧的 repl 并创建新的
        delete repl;
        repl = TriageRepl::create_repl(&entry_list, assoc);
        
        std::cout << "After move, new repl = " << repl << std::endl;
    }

    // 移动赋值运算符
    TriageOnchip& operator=(TriageOnchip&& other) noexcept {
        if (this != &other) {
            std::cout << "TriageOnchip move assignment operator called" << std::endl;
            
            // 删除现有资源
            delete repl;
            
            // 移动资源
            num_sets = other.num_sets;
            assoc = other.assoc;
            index_mask = other.index_mask;
            entry_list = std::move(other.entry_list);
            repl = other.repl;
            
            // 防止原对象析构时删除 repl
            other.repl = nullptr;
            
            // 为新的 entry_list 创建新的 repl
            delete repl;
            repl = TriageRepl::create_repl(&entry_list, assoc);
        }
        return *this;
    }

    // ~TriageOnchip() {
    //     if (repl) {
    //         delete repl;
    //     }
    // }

    void set_conf(TriageConfig* config) {
        delete repl;
        repl = nullptr;
        
        num_sets = config->on_chip_set;
        assoc = config->on_chip_assoc;
        index_mask = num_sets - 1;
        
        entry_list.clear();
        entry_list.resize(num_sets);
        
        repl = TriageRepl::create_repl(&entry_list, assoc);
        std::cout << "TriageOnchip: Configuration updated" << std::endl;
    }

    
  void update(uint64_t prev_addr, Metadata next_entry, uint64_t pc, bool update_repl, map<uint64_t, bool>& in_metadata, uint64_t& useful_entry_number, uint64_t& useless_entry_number);
  Metadata get_next_entry(uint64_t prev_addr, uint64_t pc, bool update_stats);
  int increase_confidence(uint64_t addr);
  int decrease_confidence(uint64_t addr);

  void print_stats();
  uint32_t get_assoc();
};

class triage: public champsim::modules::prefetcher{
    uint64_t metadata_hit = 0;
    uint64_t no_predict = 0;

    //statistics the metadata useful entry rate
    map<uint64_t, bool> in_metadata;
    uint64_t useful_entry_number = 0;
    uint64_t useless_entry_number = 0;

    TriageTrainingUnit training_unit;

    TriageConfig conf;

    int lookahead = 1, degree = 1;

    void train(uint64_t pc, uint64_t addr, bool hit);
    void predict(uint64_t pc, uint64_t addr, bool hit);

    // Stats
    uint64_t same_addr{0}, new_addr{0}, new_stream{0};
    uint64_t no_next_addr{0}, conf_dec_retain{0}, conf_dec_update{0}, conf_inc{0};
    uint64_t predict_count{0}, trigger_count{0};
    uint64_t spatial{0}, temporal{0};
    uint64_t total_assoc{0};
    uint64_t last_address{0};

    std::vector<uint64_t> next_addr_list;

    public:
    TriageOnchip on_chip_data;
        //Triage();
        // void test();
        void set_conf(TriageConfig *config);
        void calculatePrefetch(uint64_t pc, uint64_t addr,
                bool cache_hit, uint64_t *prefetch_list,
                int max_degree);
        void print_stats();
        uint32_t get_assoc();

    
        using champsim::modules::prefetcher::prefetcher;

        uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                            uint32_t metadata_in);
        uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
        void prefetcher_final_stats(){
            cout<<"Metadata_Hit: "<<metadata_hit<<endl;
            cout<<"Metadata_Miss: "<<no_predict<<endl;
            for (const auto &item : in_metadata){
            if (item.second == true) useful_entry_number++;
            else useless_entry_number++;
        }
        cout<<"Useful_Entry: "<<useful_entry_number<<endl;
        cout<<"Useless_Entry: "<<useless_entry_number<<endl;
        }

        
        
};

#endif // TRIAGE_H__