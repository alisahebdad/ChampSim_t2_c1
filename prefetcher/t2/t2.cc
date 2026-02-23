#include <cassert>
#include "t2_prefetcher.h"
#include "cache.h"
#include <iostream>

#include "memory_trace.h"

void CACHE::prefetcher_initialize() {
}


uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  
//  if (cache_hit)
//    debug::debug(CACHE::NAME,current_cycle,ip,addr,type);
  if (debug::inLoop)
    debug::total_access++;// count all cpu data request ;  

  // return metadata_in;
  if (t2::loop_branch_register.valid){
    //std::cout << t2::loop_branch_register.ip << "  " << t2::loop_branch_register.branch_target << std::endl;
    if ((t2::loop_branch_register.ip > ip && t2::loop_branch_register.branch_target < ip) ){
      //std::cout << "ADDR = " << std::hex << addr << " ip = " << ip << std::endl;  
      //t2::print_sit();
      auto prefetch_candidate = t2::update_sit(ip,addr);
      if (prefetch_candidate){
        //std::cout << "prefetc
        //h memory : " << prefetch_candidate << std::endl;
        prefetch_line(prefetch_candidate,true,metadata_in);
      }
    }
  }





  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}
