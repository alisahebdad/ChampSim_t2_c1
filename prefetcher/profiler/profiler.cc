#include "cache.h"
#include <exception>
#include <iostream>
#include <fstream>
#include "memory_trace.h"
#include <vector>

const uint64_t cycle_per = 130000;

std::vector <int> access_per_time;

void CACHE::prefetcher_initialize() {
  debug::memory_access_count = 0;
  access_per_time.push_back(0);
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  
  // return metadata_in;
  int miliseconds_ = current_cycle/cycle_per;
    if (!cache_hit){
    if (debug::inLoop)
      debug::memory_access_count++;
    //std::cout << current_cycle << " " << miliseconds_ << std::endl;
    while (access_per_time.size() <= miliseconds_){
      // std::cout << miliseconds_ << std::endl;
      access_per_time.push_back(0);
    }
    access_per_time[miliseconds_]++; 
  }
 
  /*
  if (cache_hit)
    debug::debug(CACHE::NAME,current_cycle,ip,addr,type);
  else 
    debug::debug("MEMORY",current_cycle,ip,addr,type);
*/
    return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{

  //fout << current_cycle << "," << addr << "," << 0 << "," << 3 << std::endl;
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {
  std::cout << "Profiler installed \n" ;
  std::cout << "memory access in inner loop : " << debug::memory_access_count << std::endl;
  for (std::size_t i = 0;i< access_per_time.size();++i)
    std::cout <<  i << "," << access_per_time[i] << std::endl;
}
