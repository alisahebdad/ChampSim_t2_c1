#ifndef T2_PREFETCHER_H
#define T2_PREFETCHER_H
#pragma once
#include <deque>
#include <vector>
#include <cstdint>
#include <iostream>
#include "instruction.h"
#include <set>
namespace t2{

 
  struct sit_entry{
    int state;
    uint64_t mPC;
    uint64_t lastAddr;
    int64_t delta;
    uint64_t lastPref;
    int age;
  };

 struct branch_target {
    uint64_t ip;
    uint64_t branch_target;
    bool valid;
    int count;
  };   
  

  extern std::deque<uint64_t> function_stack;  
  extern std::vector<std::pair<uint64_t,int>> blacklist; // Pair < Ip, age> 
  extern std::vector <sit_entry> sit;   
  extern std::set<uint64_t> non_stride;

  extern bool log_write ;
  extern branch_target loop_branch_register;

  
  bool is_in_blacklist(uint64_t ip);
  void branch_handler(uint64_t ip, uint8_t branch_type, uint64_t branch_target) ;
  void update_blacklist(uint64_t ip);
  void print_blacklist();
  

  int  search_sit(uint64_t mPC);
  void remove_sit(std::size_t i);
  void print_sit_entry(sit_entry e);
  uint64_t update_sit(uint64_t ip,uint64_t addr);
  void print_sit();




}
#endif
