#pragma once  // 防止重复包含

#include <vector>
#include <unordered_map>
#include <iostream>
#include <set>
#include <random>


#include "address.h"
#include "champsim.h"
#include "modules.h"
#include "cache.h"

using namespace std;

/*Define Irregular memory access matching prefetcher*/
#define CACHE_SET 4096  /*DEPRECATED*/
#define CACHE_WAY 10   /*DEPRECATED*/
#define HT_SET 4096
#define HT_WAY 48

#define KD_SIZE 32
#define TU_SIZE 16
#define IP_MASK (0xFF)



// define the required data sructure for croage
template <class T>
class LRUCache // using LRUcache to record last recent access
{
private:
  struct DLinkedNode {
    uint64_t key;
    T value;
    DLinkedNode* pre;
    DLinkedNode* post;
  };
  unordered_map<uint64_t, DLinkedNode*> cache;
  int count;
  int capacity;
  DLinkedNode* head;
  DLinkedNode* tail;

public:
  LRUCache(int capacity)
  {
    this->count = 0;
    this->capacity = capacity;

    head = new DLinkedNode();
    head->pre = nullptr;

    tail = new DLinkedNode();
    tail->post = nullptr;

    head->post = tail;
    tail->pre = head;
  }

  // 拷贝构造函数
  LRUCache(const LRUCache& other) : count(other.count), capacity(other.capacity) {
    // 创建新的头尾节点
    head = new DLinkedNode();
    head->pre = nullptr;
    tail = new DLinkedNode();
    tail->post = nullptr;
    head->post = tail;
    tail->pre = head;

    // 深拷贝链表节点
    DLinkedNode* curr = other.head->post;
    while (curr != other.tail) {
        DLinkedNode* newNode = new DLinkedNode();
        newNode->key = curr->key;
        newNode->value = curr->value;
        cache[curr->key] = newNode;
        addNode(newNode);
        curr = curr->post;
    }
}

    // 移动构造函数
    LRUCache(LRUCache&& other) noexcept
    : count(other.count)
    , capacity(other.capacity)
    , head(other.head)
    , tail(other.tail)
    , cache(std::move(other.cache)) 
    {
    // 重置other的状态
    other.head = nullptr;
    other.tail = nullptr;
    other.count = 0;
    }

    // 移动赋值运算符
    LRUCache& operator=(LRUCache&& other) noexcept {
    if (this != &other) {
        // 清理当前资源
        clear();

        // 移动资源
        count = other.count;
        capacity = other.capacity;
        head = other.head;
        tail = other.tail;
        cache = std::move(other.cache);

        // 重置other
        other.head = nullptr;
        other.tail = nullptr;
        other.count = 0;
    }
    return *this;
    }

    ~LRUCache(){
        clear();
    }

// 清理函数
void clear() {
    if (head != nullptr) {
        DLinkedNode* current = head->post;
        while (current != tail) {
            DLinkedNode* next = current->post;
            delete current;
            current = next;
        }
        delete head;
        delete tail;
        cache.clear();
    }
}

  T* get(uint64_t key)
  {
    if (cache.find(key) == cache.end()) {
      return nullptr;
    }
    DLinkedNode* node = cache[key];
    moveToHead(node);

    return &(node->value);
  }

  T set(uint64_t key, T value)
  {
    if (cache.find(key) == cache.end()) {
      DLinkedNode* newNode = new DLinkedNode();
      newNode->key = key;
      newNode->value = value;

      cache[key] = newNode;
      addNode(newNode);

      ++count;

      if (count > capacity) {
        DLinkedNode* tailNode = popTail();
        cache.erase(tailNode->key);
        --count;
        T record = tailNode->value;
        delete tailNode;
        return record;
      }
    } else {
      DLinkedNode* node = cache[key];
      node->value = value;
      moveToHead(node);
    }
    return value;
  }

  DLinkedNode* begin() { return head; }

  DLinkedNode* end() { return tail; }

private:
  void addNode(DLinkedNode* node)
  {
    node->pre = head;
    node->post = head->post;

    head->post->pre = node;
    head->post = node;
  }

  void removeNode(DLinkedNode* node)
  {
    DLinkedNode* pre = node->pre;
    DLinkedNode* post = node->post;

    pre->post = post;
    post->pre = pre;
  }

  void moveToHead(DLinkedNode* node)
  {
    removeNode(node);
    addNode(node);
  }

  DLinkedNode* popTail()
  {
    DLinkedNode* res = tail->pre;
    removeNode(res);
    return res;
  }
};

template <class T>
class FIFOCache // using FIFOcache to replace sampler entry
{
private:
  struct DLinkedNode {
    uint64_t key;
    T value;
    DLinkedNode* pre;
    DLinkedNode* post;
  };
  unordered_map<uint64_t, DLinkedNode*> cache;
  int count;
  int capacity;
  DLinkedNode* head;
  DLinkedNode* tail;

public:
  FIFOCache(int capacity)
  {
    this->count = 0;
    this->capacity = capacity;

    head = new DLinkedNode();
    head->pre = nullptr;

    tail = new DLinkedNode();
    tail->post = nullptr;

    head->post = tail;
    tail->pre = head;
  }

  // 拷贝构造函数
  FIFOCache(const FIFOCache& other) : count(other.count), capacity(other.capacity) {
    // 创建新的头尾节点
    head = new DLinkedNode();
    head->pre = nullptr;
    tail = new DLinkedNode();
    tail->post = nullptr;
    head->post = tail;
    tail->pre = head;

    // 深拷贝链表节点
    DLinkedNode* curr = other.head->post;
    while (curr != other.tail) {
        DLinkedNode* newNode = new DLinkedNode();
        newNode->key = curr->key;
        newNode->value = curr->value;
        cache[curr->key] = newNode;
        addNode(newNode);
        curr = curr->post;
    }
}

// 移动构造函数
FIFOCache(FIFOCache&& other) noexcept
    : count(other.count)
    , capacity(other.capacity)
    , head(other.head)
    , tail(other.tail)
    , cache(std::move(other.cache)) 
{
    // 重置other的状态
    other.head = nullptr;
    other.tail = nullptr;
    other.count = 0;
}

// 移动赋值运算符
FIFOCache& operator=(FIFOCache&& other) noexcept {
    if (this != &other) {
        // 清理当前资源
        clear();

        // 移动资源
        count = other.count;
        capacity = other.capacity;
        head = other.head;
        tail = other.tail;
        cache = std::move(other.cache);

        // 重置other
        other.head = nullptr;
        other.tail = nullptr;
        other.count = 0;
    }
    return *this;
}

  ~FIFOCache(){
    clear();
  }

  // 清理函数
  void clear() {
    if (head != nullptr) {
        DLinkedNode* current = head->post;
        while (current != tail) {
            DLinkedNode* next = current->post;
            delete current;
            current = next;
        }
        delete head;
        delete tail;
        cache.clear();
    }
}

  class Iterator {
  private:
    DLinkedNode* node;
  public:
    Iterator(DLinkedNode* pNode) : node(pNode) {}

    T& operator*() const { return node->value; }
    T* operator->() { return &(node->value); }

    // Prefix increment
    Iterator& operator++() {
      node = node->post;
      return *this;
    }

    // Postfix increment
    Iterator operator++(int) {
      Iterator iterator = *this;
      ++(*this);
      return iterator;
    }

    bool operator==(const Iterator& other) const { return node == other.node; }
    bool operator!=(const Iterator& other) const { return node != other.node; }
  };


  T* get(uint64_t key)
  {
    if (cache.find(key) == cache.end()) {
      return nullptr;
    }
    DLinkedNode* node = cache[key];
    return &(node->value);
  }

  T set(uint64_t key, T value)
  {
    if (cache.find(key) == cache.end()) {
      DLinkedNode* newNode = new DLinkedNode();
      newNode->key = key;
      newNode->value = value;

      cache[key] = newNode;
      addNode(newNode);

      ++count;

      if (count > capacity) {
        DLinkedNode* tailNode = popTail();
        cache.erase(tailNode->key);
        --count;
        T record = tailNode->value;
        delete tailNode;
        return record;
      }
    } else {
      DLinkedNode* node = cache[key];
      node->value = value;
    }
    return value;
  }

  Iterator begin() { return Iterator(head->post); }
  Iterator end() { return Iterator(tail); }

private:
  void addNode(DLinkedNode* node)
  {
    node->pre = head;
    node->post = head->post;

    head->post->pre = node;
    head->post = node;
  }

  void removeNode(DLinkedNode* node)
  {
    DLinkedNode* pre = node->pre;
    DLinkedNode* post = node->post;

    pre->post = post;
    post->pre = pre;
  }

  void moveToHead(DLinkedNode* node)
  {
    removeNode(node);
    addNode(node);
  }

  DLinkedNode* popTail()
  {
    DLinkedNode* res = tail->pre;
    removeNode(res);
    return res;
  }
};

template <class T>
class CroageRepl {
private:
    struct DLinkedNode {
        uint64_t key;
        T value;
        int confidence;
        int rpl;
        DLinkedNode* pre;
        DLinkedNode* post;
    };
    const int max_conf = 1;
    const int max_rpl = 3;
    unordered_map<uint64_t, DLinkedNode*> cache;
    int count;
    int capacity;
    DLinkedNode* head;
    DLinkedNode* tail;

public:
    // 构造函数
    CroageRepl(int capacity) 
        : count(0)
        , capacity(capacity)
        , head(new DLinkedNode())
        , tail(new DLinkedNode()) 
    {
        head->pre = nullptr;
        tail->post = nullptr;
        head->post = tail;
        tail->pre = head;
    }

    // 拷贝构造函数
    CroageRepl(const CroageRepl& other) 
        : count(0)  // 将在添加节点时递增
        , capacity(other.capacity)
        , head(new DLinkedNode())
        , tail(new DLinkedNode()) 
    {
        head->pre = nullptr;
        tail->post = nullptr;
        head->post = tail;
        tail->pre = head;

        try {
            DLinkedNode* curr = other.head->post;
            while (curr != other.tail) {
                DLinkedNode* newNode = new DLinkedNode();
                newNode->key = curr->key;
                newNode->value = curr->value;
                newNode->confidence = curr->confidence;
                newNode->rpl = curr->rpl;
                
                cache[curr->key] = newNode;
                addNode(newNode);
                count++;
                
                curr = curr->post;
            }
        } catch (...) {
            clear();
            throw;
        }
    }

    // 移动构造函数
    CroageRepl(CroageRepl&& other) noexcept 
        : count(other.count)
        , capacity(other.capacity)
        , cache(std::move(other.cache))
        , head(other.head)
        , tail(other.tail) 
    {
        other.count = 0;
        other.capacity = 0;
        other.head = nullptr;
        other.tail = nullptr;
    }

    // 移动赋值运算符
    CroageRepl& operator=(CroageRepl&& other) noexcept {
        if (this != &other) {
            // 清理当前资源
            clear();

            // 转移资源
            count = other.count;
            capacity = other.capacity;
            cache = std::move(other.cache);
            head = other.head;
            tail = other.tail;

            // 重置other
            other.count = 0;
            other.capacity = 0;
            other.head = nullptr;
            other.tail = nullptr;
        }
        return *this;
    }

    // 析构函数
    ~CroageRepl() {
        clear();
    }

private:
    // 清理函数
    void clear() {
        if (head != nullptr) {
            DLinkedNode* current = head->post;
            while (current != tail) {
                DLinkedNode* next = current->post;
                delete current;
                current = next;
            }
            delete head;
            delete tail;
            head = nullptr;
            tail = nullptr;
            cache.clear();
            count = 0;
        }
    }

public:
    T* get(uint64_t key) {
        auto it = cache.find(key);
        if (it == cache.end()) {
            return nullptr;
        }
        DLinkedNode* node = it->second;
        if (node->rpl > 0) {
            node->rpl -= 1;
        }
        return &(node->value);
    }

    uint64_t set(uint64_t key, T value, bool useful_tu) {
        auto it = cache.find(key);
        if (it == cache.end()) {
            if (count >= capacity) {
                DLinkedNode* victim = popVictim();
                if (victim != nullptr) {
                    uint64_t pop_key = victim->key;
                    cache.erase(pop_key);
                    delete victim;
                    count--;
                    return pop_key;
                }
            }

            DLinkedNode* newNode = new DLinkedNode();
            newNode->key = key;
            newNode->value = value;
            newNode->confidence = max_conf;
            newNode->rpl = max_rpl;
            if (useful_tu) {
                newNode->rpl--;
            }
            
            try {
                cache[key] = newNode;
                addNode(newNode);
                count++;
            } catch (...) {
                delete newNode;
                throw;
            }
        } else {
            DLinkedNode* node = it->second;
            node->value = value;
        }
        return 0;
    }

    // Iterator class for CroageRepl
    class Iterator {
      private:
          DLinkedNode* node;
      public:
          Iterator(DLinkedNode* pNode) : node(pNode) {}
  
          std::pair<uint64_t, T> operator*() const { 
              return {node->key, node->value}; 
          }
  
          // Prefix increment
          Iterator& operator++() {
              node = node->post;
              return *this;
          }
  
          // Postfix increment
          Iterator operator++(int) {
              Iterator iterator = *this;
              ++(*this);
              return iterator;
          }
  
          bool operator==(const Iterator& other) const { return node == other.node; }
          bool operator!=(const Iterator& other) const { return node != other.node; }
      };
  
      // Begin and end methods for iteration
      Iterator begin() { return Iterator(head->post); }
      Iterator end() { return Iterator(tail); }

private:
    void addNode(DLinkedNode* node) {
        if (head != nullptr && node != nullptr) {
            node->pre = head;
            node->post = head->post;
            head->post->pre = node;
            head->post = node;
        }
    }

    void removeNode(DLinkedNode* node) {
        if (node != nullptr && node->pre != nullptr && node->post != nullptr) {
            node->pre->post = node->post;
            node->post->pre = node->pre;
        }
    }

    void moveToHead(DLinkedNode* node) {
        if (node != nullptr) {
            removeNode(node);
            addNode(node);
        }
    }

    DLinkedNode* popVictim() {
        if (tail == nullptr || head == nullptr) return nullptr;
        
        DLinkedNode* res = tail->pre;
        if (res == head) return nullptr;
        
        if (res->rpl < max_rpl) {
            res->rpl += 1;
            moveToHead(res);
            return popVictim();
        }
        
        removeNode(res);
        return res;
    }
};

struct KeyDetectEntry
{
    // find the PC that often makes cache miss
    KeyDetectEntry(uint64_t pc=0, uint64_t issue=0, uint64_t miss=0):pc(pc), issue_count(issue), miss_count(miss){}
    uint64_t pc;
    uint64_t issue_count;
    uint64_t miss_count;
};

struct BOP{
    // find best offset
    static const int max_offset = 63;
    // const int threshhold = 5;
    BOP(): refresh(0), offsets(vector<uint64_t>(max_offset*2+1)), prefetch(vector<uint8_t>(max_offset*2+1)){}
    uint64_t refresh = 0;
    uint64_t count = 0;
    vector<uint64_t> offsets = vector<uint64_t>(max_offset*2+1);
    vector<uint8_t> prefetch = vector<uint8_t>(max_offset*2+1);

    // update the offsets to calculate the prefetch candidates
    void update(int offset){
        if (abs(offset)>max_offset) return;
        offsets[offset+max_offset]++;
        refresh++;
        count++;
        if (count>511){
            for(auto& i:prefetch){
                i = 0;
            }
            count = 0;
        }
        if (refresh>31){
            for(int i = 0; i<max_offset*2+1; ++i){
                if (offsets[i]*2>=refresh) prefetch[i] = 1;
                offsets[i] = 0;
            }
            refresh = 0;
        }
    }

    vector<int> get_offsets(){
        vector<int> result;
        for(int i = 0; i<max_offset*2+1; ++i){
            if (prefetch[i]) {
                result.push_back(i-max_offset);
            }
        }
        return result;
    }
};

// detect the key pc that causes cache miss frequently
class KeyDetect{
    FIFOCache<KeyDetectEntry> entry_list;
    uint64_t total_miss;
    // count the pc miss more than 50% and the key pc total miss proportion more than 50%
    set<uint32_t> miss_pc;
    set<uint32_t> key_pc;

public:
    KeyDetect(int capacity): entry_list(FIFOCache<KeyDetectEntry>(capacity)), total_miss(0){}

    bool update(uint64_t pc, uint8_t cache_hit){
        auto entry = entry_list.get(pc);
        if (!entry){
            entry_list.set(pc, KeyDetectEntry(pc, 1, !cache_hit));
            return 0;
        }
        else{
            entry->issue_count++;
            if (!cache_hit){
                entry->miss_count++;
                total_miss++;
            }
            if (entry->miss_count*2>entry->issue_count) return 1;
            if (entry->miss_count*8>total_miss) return 1;
        }
        if (total_miss>511){
            for(auto& i: entry_list){
                if(i.miss_count*8>total_miss) key_pc.insert(i.pc);
                //if(i.miss_count*2>i.issue_count) miss_pc.insert(i.pc);
                i.issue_count = 0;
                i.miss_count = 0;
            }
            total_miss = 0;
        }
        return 0;
    }

    void print_status(){
        cout<<"The number of pc miss rate surpass 50% is "<< miss_pc.size()<<'\n';
        cout<<"The number of pc incurs main cache misses is "<< key_pc.size()<<'\n';
    }
};

struct TrainUnitEntry{
    TrainUnitEntry():pc(0), last_addr(0), cur_addr(0), total_evict(0), useful_evict(0), useful_tu(false),bo(BOP()){}
    TrainUnitEntry(uint64_t pc, uint64_t cur_addr):pc(pc), last_addr(0), cur_addr(cur_addr), total_evict(0), useful_evict(0),useful_tu(false),bo(BOP()){}
    uint64_t pc;
    uint64_t last_addr;
    uint64_t cur_addr;
    uint16_t total_evict;
    uint16_t useful_evict;
    bool useful_tu;
    BOP bo;
};

class croage: public champsim::modules::prefetcher{
    
    struct corres_cache {
        uint64_t addr = 0;
        uint8_t pf = 0;     // Is this prefetched
    };

    // // Calculate the access data of croage
    uint64_t no_predict = 0;
    uint64_t metadata_hit = 0;

    // // statistics the metadata useful entry rate
    map<uint64_t, bool> in_metadata;
    uint64_t useful_entry_number = 0;
    uint64_t useless_entry_number = 0;

    static constexpr int cache_set = 2048;
    static constexpr int cache_way = 16;
    const int bit_mask = HT_SET - 1;
    const int max_degree = 4;

    // 直接在成员声明时初始化
    KeyDetect DetectUnit{KD_SIZE};
    LRUCache<TrainUnitEntry> TrainUnit{TU_SIZE};
    //vector<CroageRepl<uint64_t>> MetaData(HT_SET, CroageRepl<uint64_t>(HT_WAY));
    vector<vector<corres_cache>> ccache{cache_set, vector<corres_cache>(cache_way)};
    map<uint64_t, uint64_t> CAM;

    vector<CroageRepl<uint64_t>> initialize_metadata_vector() {
      vector<CroageRepl<uint64_t>> temp;
      temp.reserve(HT_SET);
      for(int i = 0; i < HT_SET; ++i) {
          temp.emplace_back(HT_WAY);
      }
      return temp;
  }

  vector<CroageRepl<uint64_t>> MetaData = initialize_metadata_vector();

    // try best offset for regular prefetch
    BOP bo;
    uint64_t last_addr = 0;

    // Access tracking for dynamic metadata sizing
    const uint64_t TRACKING_WINDOW = 262144; // 2^18 accesses
    uint64_t access_count = 0;
    uint64_t miss_count = 0;
    uint64_t prefetch_hit_count = 0;
    uint64_t metadata_hit_count = 0;
    uint64_t current_window = 0;
    uint32_t increase_metadata = 0;
    uint32_t decrease_metadata = 0;
    uint32_t maintain_metadata = 0;
    
    // // Metadata resizing parameters
    // const double RESIZE_THRESHOLD_UP = 0.6;   // If useful metadata hits > 60% of prefetch hits, increase size
    // const double RESIZE_THRESHOLD_DOWN = 0.3; // If useful metadata hits < 30% of prefetch hits, decrease size
    // const double MISS_IMPROVEMENT_THRESHOLD = 0.05; // 5% improvement threshold
    uint64_t current_metadata_size = HT_WAY;  // Start with the default size
    const uint64_t MAX_METADATA_SIZE = 96;    // Maximum allowed metadata ways
    const uint64_t MIN_METADATA_SIZE = 12;    // Minimum allowed metadata ways

    // PID controller parameters
    const double ALPHA = 0.6;  // Prefetch utility weight
    const double BETA = -0.3;   // Miss sensitivity weight  
    const double GAMMA = 0.1;  // Miss change rate weight
    const double THETA_PLUS = 0.5;   // Positive adjustment threshold
    const double THETA_MINUS = -0.25; // Negative adjustment threshold
    const double TAU = 1.2;    // Miss increase threshold
    
    // Additional state tracking
    double previous_utility = 0.0;
    uint64_t previous_miss_count = 0;
    double previous_miss_rate = 0.0;
    double previous_delta_miss = 0.0;
    
    // Stability constraints
    const int MAX_NO_IMPROVEMENT_WINDOWS = 2;
    const double IMPROVEMENT_THRESHOLD = 0.05; // 5% improvement threshold
    int no_improvement_count = 0;
    
    // Exponential moving average for miss rate
    double smoothed_miss_rate = 0.0;
    const double EMA_ALPHA = 0.7; // Weight for current miss rate

    CACHE* llc_cache = nullptr;

    void set_llc_reference(CACHE* llc) {
        llc_cache = llc;
        
        // // set original partition ways
        // if (llc_cache != nullptr) {
        //     uint32_t occupied_ways = current_metadata_size / 12;
        //     uint32_t available_ways = llc_cache->NUM_WAY - occupied_ways;
        //     llc_cache->set_available_ways(available_ways);
            
        //     if constexpr (champsim::debug_print) {
        //         std::cout << "[CROAGE] LLC initial partition: metadata_ways=" 
        //                   << occupied_ways << ", available_ways=" 
        //                   << available_ways << std::endl;
        //     }
        // }
    }

    // // Historical window data for more informed decisions
    // struct WindowStats {
    //   uint64_t miss_count = 0;
    //   uint64_t prefetch_hit_count = 0;
    //   uint64_t metadata_hit_count = 0;
    //   int8_t resize_decision = 0; // -1 for decrease, 0 for no change, 1 for increase
    // };

    // WindowStats previous_window;
    // WindowStats pre_previous_window;

    // calculate data
    uint64_t tem_hit_num = 0;
    uint64_t OoO_update = 0;

    // Function to resize metadata tables
    void resize_metadata(int8_t direction) {
      if (direction==1){
        increase_metadata++;
      }
      else if (direction==-1){
        decrease_metadata++;
      }
      else{
        maintain_metadata++;
      }

      uint64_t new_size = current_metadata_size;
      
      if (direction > 0 && current_metadata_size < MAX_METADATA_SIZE) {
          new_size = current_metadata_size + 12; // Increase by 4 ways
          
      } else if (direction < 0 && current_metadata_size > MIN_METADATA_SIZE) {
          new_size = current_metadata_size - 12; // Decrease by 4 ways
          
      } else {
          return; // No change needed
      }
      
      if (new_size != current_metadata_size) {
          // Create new metadata tables with the new size
          vector<CroageRepl<uint64_t>> new_metadata;
          new_metadata.reserve(HT_SET);
          for (int i = 0; i < HT_SET; ++i) {
              new_metadata.emplace_back(new_size);
          }
          
          // Copy data from old metadata tables to new ones
          for (int i = 0; i < HT_SET; ++i) {
            // Directly iterate through entries in the old metadata table
            for (auto entry : MetaData[i]) {
                // Each entry is a pair of {key, value}
                uint64_t addr = entry.first;
                uint64_t value = entry.second;
                
                // Copy the entry to the new metadata table
                new_metadata[i].set(addr, value, false);
            }
          }
          
          // Replace old metadata with new metadata
          MetaData = std::move(new_metadata);
          current_metadata_size = new_size;

          if (llc_cache != nullptr) {
              uint32_t occupied_ways = current_metadata_size / 12;
              uint32_t available_ways = llc_cache->NUM_WAY - occupied_ways;
              llc_cache->set_available_ways(available_ways);
              
              if constexpr (champsim::debug_print) {
                  std::cout << "[CROAGE] LLC partition updated: metadata_ways=" 
                            << occupied_ways << ", available_ways=" 
                            << available_ways << std::endl;
              }
          }
          
          // Log the resize operation (in a real system, you'd want to log this)
          // std::cout << "Resized metadata to " << new_size << " ways" << std::endl;
      }
  }
  
  // Function1 to evaluate tracking window and make sizing decisions
  // void evaluate_window() {
  //   int8_t resize_decision = 0; // Default: no change
        
  //   // Store current stats before evaluating
  //   WindowStats current_stats = {
  //       miss_count,
  //       prefetch_hit_count,
  //       metadata_hit_count,
  //       0 // Decision will be set later
  //   };
    
  //   // Initial heuristic based on metadata effectiveness
  //   if (prefetch_hit_count > 0) {
  //       double metadata_effectiveness = static_cast<double>(prefetch_hit_count) / miss_count;
        
  //       if (metadata_effectiveness > RESIZE_THRESHOLD_UP) {
  //           // Metadata is very effective, potential increase
  //           resize_decision = 1;
  //       } else if (metadata_effectiveness < RESIZE_THRESHOLD_DOWN) {
  //           // Metadata is not very effective, potential decrease
  //           resize_decision = -1;
  //       }
  //   }
    
  //   // Consider history to refine decision
  //   if (current_window > 1) {
  //       // Check if the last decision made a positive impact
  //       if (previous_window.resize_decision != 0) {
  //           // If we increased size last time
  //           if (previous_window.resize_decision > 0) {
  //               double prev_miss_rate = static_cast<double>(previous_window.miss_count) / TRACKING_WINDOW;
  //               double current_miss_rate = static_cast<double>(miss_count) / TRACKING_WINDOW;
                
  //               // If miss rate didn't improve meaningfully after increasing size
  //               if (current_miss_rate < prev_miss_rate * (1.0 + MISS_IMPROVEMENT_THRESHOLD)) {
  //                   // Revert to previous size or keep the same
  //                   resize_decision = 0;
  //               }
  //           }
  //           // If we decreased size last time
  //           else if (previous_window.resize_decision < 0) {
  //               double prev_miss_rate = static_cast<double>(previous_window.miss_count) / TRACKING_WINDOW;
  //               double current_miss_rate = static_cast<double>(miss_count) / TRACKING_WINDOW;
                
  //               // If miss rate got significantly worse after decreasing size
  //               if (current_miss_rate > prev_miss_rate * (1.0 - MISS_IMPROVEMENT_THRESHOLD)) {
  //                   // Revert and increase size
  //                   resize_decision = 1;
  //               }
  //           }
  //       }
        
  //       // Avoid oscillation by checking previous two windows
  //       if (current_window > 2) {
  //           if (pre_previous_window.resize_decision == 1 && previous_window.resize_decision == -1) {
  //               // We're oscillating, stick with current size for stability
  //               resize_decision = 0;
  //           }
  //       }
  //   }
    
  //   // Apply the decision
  //   resize_metadata(resize_decision);
    
  //   // Update window history
  //   current_stats.resize_decision = resize_decision;
  //   pre_previous_window = previous_window;
  //   previous_window = current_stats;
    
  //   // Reset counters for next window
  //   miss_count = 0;
  //   prefetch_hit_count = 0;
  //   metadata_hit_count = 0;
  //   current_window++;
  // }

  // Function to evaluate tracking window and make sizing decisions
  void evaluate_window() {
    // Calculate metrics for this window
    double miss_rate = static_cast<double>(miss_count) / TRACKING_WINDOW;
    double utility = 0.0;
    
    // Calculate prefetch utility (η)
    if (miss_count > 0) {
        // Add small epsilon to avoid division by zero
        utility = static_cast<double>(prefetch_hit_count) / miss_count;
        //(miss_count + 1e-6);
    }
    
    // Calculate miss rate sensitivity (ΔM)
    double delta_miss = 0.0;
    if (current_window > 0) {
        delta_miss = static_cast<double>(miss_count - previous_miss_count) / access_count ;
    }
    
    // Apply EMA smoothing to miss rate
    smoothed_miss_rate = EMA_ALPHA * miss_rate + (1.0 - EMA_ALPHA) * smoothed_miss_rate;
    
    // Calculate adjustment using PID-like formula
    double delta_k = ALPHA * utility + 
                     BETA * delta_miss + 
                     GAMMA * (delta_miss - previous_delta_miss);
    
    // Decision making
    int8_t resize_decision = 0; // Default: no change
    
    // Check condition for increasing metadata size
    if (delta_k > THETA_PLUS && 
      static_cast<double>(prefetch_hit_count)/access_count > static_cast<double>(current_metadata_size) / (MAX_METADATA_SIZE+12)) {
        resize_decision = 1; // Increase
    }
    // Check conditions for decreasing metadata size
    else if (delta_k < THETA_MINUS || miss_rate > TAU * previous_miss_rate ) {
        resize_decision = -1; // Decrease
    }
    
    // Apply stability constraints
    if (current_window > 0) {
        // Check if we're seeing meaningful improvement
        double relative_improvement = previous_miss_rate - miss_rate;
        
        if (relative_improvement < IMPROVEMENT_THRESHOLD) {
            no_improvement_count++;
            
            // If we haven't seen improvement for several windows, maintain current size
            if (no_improvement_count >= MAX_NO_IMPROVEMENT_WINDOWS) {
                resize_decision = 0;
            }
        } else {
            // Reset the counter if we see improvement
            no_improvement_count = 0;
        }
    }
    
    // Implement cold start strategy:  for initial phase
    // if (current_window < 10 && current_metadata_size != HT_WAY) {
    //     resize_decision = (current_metadata_size < HT_WAY) ? 1 : -1;
    // }
    
    // Apply resource penalty for high k values to prevent excessive metadata usage
    // if (current_metadata_size > 48 && resize_decision > 0) {
    //     // Make it harder to increase when already using significant resources
    //     if (delta_k < THETA_PLUS * 1.5) {
    //         resize_decision = 0;
    //     }
    // }
    
    // Apply the decision
    resize_metadata(resize_decision);
    
    // Store current metrics for next comparison
    previous_utility = utility;
    previous_miss_count = miss_count;
    previous_delta_miss = delta_miss;
    previous_miss_rate = miss_rate;
    
    // Reset counters for next window
    miss_count = 0;
    prefetch_hit_count = 0;
    
    current_window++;
    
    // // Log window evaluation (for debugging)
    // std::cout << "Window " << current_window 
    //           << ": Miss rate=" << miss_rate
    //           << ", Utility=" << utility 
    //           << ", Delta miss=" << delta_miss
    //           << ", Adjustment=" << delta_k
    //           << ", New metadata size=" << current_metadata_size << std::endl;
  }
  
  // Add to print_status function
  void print_status() {
      DetectUnit.print_status();
      //cout << "OoO interferes MetaData records: " << OoO_update << '\n';
      cout << "Metadata_Hit: " << tem_hit_num << '\n';
      cout << "Current metadata size: " << current_metadata_size/12 << " ways\n";
      cout<< "Increase, decrease and maintain metadata numbers: "<< increase_metadata<<' '<<decrease_metadata<<' '<<maintain_metadata<<'\n';
      cout << "Total windows evaluated: " << current_window << '\n';
  }

    void tem_up(uint64_t last_addr, uint64_t cur_addr, uint64_t pc,bool useful_tu){
      uint64_t set_num = (last_addr^(last_addr>>13)) & bit_mask;
      uint64_t pop_addr = MetaData[set_num].set(last_addr, cur_addr, useful_tu);
    //   if (in_metadata.find(last_addr) == in_metadata.end()){
         in_metadata[last_addr] = false;
    //   }
      if (pop_addr != 0){
        
        uint64_t pc = CAM[pop_addr];
        auto t_unit = TrainUnit.get(pc);
        if (t_unit != nullptr){
          if (useful_tu){
            t_unit->useful_evict++;
          }
          t_unit->total_evict++;
        }
        
        if (in_metadata[pop_addr] == true){
           useful_entry_number += 1;
        //   if (t_unit != nullptr){
        //     t_unit->total_evict++;
        //     t_unit->useful_evict++;
        //   }
        }
        else{
         useless_entry_number += 1;
        //   if (t_unit != nullptr){
        //     t_unit->total_evict++;
        //   }
        }
        in_metadata.erase(pop_addr);
        CAM.erase(pop_addr);
      }
      CAM[last_addr] = pc;
    }

    uint64_t tem_pre(uint64_t last_addr){
      uint64_t set_num = (last_addr^(last_addr>>13)) & bit_mask;
      uint64_t* cur_addr = MetaData[set_num].get(last_addr);
      if (cur_addr){
        in_metadata[last_addr] = true;
        metadata_hit_count++; // Track successful metadata predictions
        tem_hit_num++;
        return *cur_addr;
      } 
      return 0;
    }

public:
      

     uint8_t corres_cache_add(uint32_t set, uint32_t way, uint64_t line_addr, uint8_t pf){
          ccache[set][way].addr = line_addr;
          ccache[set][way].pf = pf;
          return ccache[set][way].pf;
      }

      uint8_t corres_cache_in(uint64_t line_addr)
      {
          for (uint32_t i = 0; i < cache_set; i++) {
              for (uint32_t ii = 0; ii < cache_way; ii++) {
              if (ccache[i][ii].addr == line_addr)
                  return 1;
              }
          }
          return 0;
      }

      uint8_t corres_cache_is_pf(uint64_t line_addr)
      {
          for (uint32_t i = 0; i < cache_set; i++) {
              for (uint32_t ii = 0; ii < cache_way; ii++) {
              if (ccache[i][ii].addr == line_addr)
                  return ccache[i][ii].pf;
              }
          }
          return 0;
      }

    void train(uint64_t pc, uint64_t addr, uint8_t cache_hit, bool useful_prefetch){

        if (DetectUnit.update(pc, cache_hit)){
          auto t_unit = TrainUnit.get(pc);
          if (!t_unit){
            TrainUnitEntry new_unit = TrainUnitEntry(pc, addr);
            TrainUnit.set(pc, new_unit);
          }
          else{
            if (!cache_hit || useful_prefetch){
              uint64_t last_addr = t_unit->cur_addr;
              t_unit->last_addr = last_addr;
              t_unit->cur_addr = addr;
              

              if (t_unit->total_evict==128){
                if (t_unit->useful_evict*16  > t_unit->total_evict*15){
                  t_unit->useful_tu = true;
                  t_unit->useful_evict = 0;
                  t_unit->total_evict = 0;
                }
                else{
                  t_unit->useful_tu = false;
                  t_unit->useful_evict = 0;
                  t_unit->total_evict = 0;
                }
              }

              if (std::abs(static_cast<int64_t>(addr) - static_cast<int64_t>(last_addr)) < 64){
                tem_up(last_addr, addr, pc,t_unit->useful_tu);    //tem module try to record the in-page pattern
                t_unit->bo.update(addr - last_addr);
              }
              else{
                tem_up(last_addr, addr,pc,t_unit->useful_tu);
              }
            }
            
          }
        }
        if (!cache_hit || corres_cache_is_pf(addr)){
          if (std::abs(static_cast<int64_t>(addr) - static_cast<int64_t>(last_addr)) < 64){
            bo.update(addr-last_addr);
          }
        }
        last_addr = addr;
    }

    vector<uint64_t> predict(uint64_t pc, uint64_t addr){
      vector<uint64_t> prefetch_candidate;
      // Recurrent time series prediction
      uint64_t next_addr = tem_pre(addr);
      // if (next_addr==0) no_predict++;
      // else metadata_hit++;
      int degree = max_degree;
      while (next_addr!=0 && degree>0){
        prefetch_candidate.push_back(next_addr);
        next_addr = tem_pre(next_addr);
        degree--;
      }
      // auto t_unit = TrainUnit.get(pc);
      // if (!t_unit) return prefetch_candidate;
      // else{
      //   auto bop = t_unit->bo.get_offsets();
      //   while(!bop.empty() && degree>0){
      //     prefetch_candidate.push_back(bop.back()+addr);
      //     bop.pop_back();
      //     degree--;
      //   }
      // }
      // auto bop = this->bo.get_offsets();
      // while(!bop.empty() && degree>0){
      //     prefetch_candidate.push_back(bop.back()+addr);
      //     bop.pop_back();
      //     degree--;
      //   }
      return prefetch_candidate;
    }


    using champsim::modules::prefetcher::prefetcher;

    uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                        uint32_t metadata_in);
    uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
    void prefetcher_cycle_operate();
    void prefetcher_final_stats();

};

template<typename T>
void removeDuplicates(std::vector<T>& vec) {
    std::sort(vec.begin(), vec.end());
    auto last = std::unique(vec.begin(), vec.end());
    vec.erase(last, vec.end());
}