#include <cassert>
#include <cstdint>
#include <iostream>
#include "cache.h"
#include <utility>
#include <vector>
#include <deque>
#include <system_error>
#include <algorithm>
#include "t2_prefetcher.h"
#include "memory_trace.h"


void CACHE::prefetcher_initialize() {
  //loop_branch_register = {0,0,0};

}

void CACHE::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {



  /* if (t2::log_write != t2::loop_branch_register.valid){
    //debug::debug("VALID",current_cycle,ip,t2::loop_branch_register.valid,0);
      t2::log_write = t2::loop_branch_register.valid;
  }
  */
  debug::inLoop = t2::loop_branch_register.valid; 

  if (branch_target == 0)
    return ;
  
  t2::branch_handler(ip,branch_type,branch_target);
 

}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{


  if (debug::inLoop)
    debug::total_access++;// count all cpu data request ;  



 //  void debug(std::string name,int clk,uint64_t ip,uint8_t type);  
  //if (cache_hit)
   // debug::debug(CACHE::NAME,current_cycle,ip,addr,type);


  /*
  if (ip == addr)
    return metadata_in;
  std::cout << "++++++++" << std::endl;

  if (loop_branch_register.valid){
    if (loop_branch_register.ip > ip && loop_branch_register.branch_target < ip){
      
      std::cout << "in loop which found " << std::hex << ip << " " << addr<< std::endl;
      auto prefetch_candidate = update_sit(ip,addr);
      

      if (prefetch_candidate){
        std::cout << "prefetch memory : " << prefetch_candidate << std::endl;
        prefetch_line(prefetch_candidate,true,metadata_in);
      }
    }else{
    

    }



  }

  assert(addr == ip);  // Invariant for instruction prefetchers
  */
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {
  std::cout << "total access in inner loop : " << debug::total_access << std::endl;
}
