#ifndef PREFETCHER_L2MISS_H
#define PREFETCHER_L2MISS_H

#include <cstdint>
#include <iostream>
#include "address.h"
#include "modules.h"
#include "champsim.h"
struct l2miss : public champsim::modules::prefetcher {


    std::string toProfilePath(const std::string& full_path) {
        size_t last_slash = full_path.find_last_of('/');
        if (last_slash == std::string::npos) {
            return "";
        }
        // 1. 找到最后一个 '/' 的位置，分离目录和文件名
        std::string dir_part = full_path.substr(0, last_slash);
        std::string file_part = full_path.substr(last_slash + 1);
        // 2. 从 dir_part 中提取最后一级目录名（即 traces-spec2017）
        size_t second_last_slash = dir_part.find_last_of('/');
        std::string trace_dir = (second_last_slash == std::string::npos)
                                    ? dir_part
                                    : dir_part.substr(second_last_slash + 1);
        // 3. 去掉 "traces-" 前缀，得到 "spec2017"
        const std::string prefix = "traces-";
        std::string suite;
        if (trace_dir.substr(0, prefix.size()) == prefix) {
            suite = trace_dir.substr(prefix.size());
        } 
        // 4. 从 file_part 中移除最后两个扩展名（.champsimtrace.xz）
        size_t last_dot = file_part.rfind('.');
        if (last_dot == std::string::npos) {
            // 没有点，整个作为 base_name
            return "";
        }
        size_t second_last_dot = file_part.rfind('.', last_dot - 1);
        std::string base_name = (second_last_dot == std::string::npos)
                                    ? file_part.substr(0, last_dot)
                                    : file_part.substr(0, second_last_dot);

        std::string profile_path = "/mnt/data/lyq/Kairos2/expr/log/"+ base_name + ".txt";
        return profile_path;

    }

  std::string trace_name;

    

  using prefetcher::prefetcher;
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);

  void prefetcher_initialize(){
   
    trace_name= toProfilePath(champsim::global_trace_name);
  }
  // void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) {}
  // void prefetcher_cycle_operate() {}
  // void prefetcher_final_stats() {}
};

#endif
