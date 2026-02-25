#include "rtlru.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

long extra_cycle_;


rtlru::rtlru(CACHE* cache) : rtlru(cache, cache->NUM_SET, cache->NUM_WAY) {
}

rtlru::rtlru(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  std::cout << "rtLRU installed \n" ;
  std::cout << "SETS : " << sets << " WAYS : " << ways << std::endl;
  rt_position = new long[sets]();

  for (int i = 0;i<6;++i){
    hit_cycle[i] = 0ll;
    miss_cycle[i] = 0ll;
  }
}

long rtlru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
  //std::cout << "find victim " << full_addr << std::endl;
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim);
}

void rtlru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{

  //std::cout << "replacement_cache_fill " <<  full_addr << std::endl;
  // Mark the way as being used on the current cycle
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void rtlru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{

  //std::cout << "update_replacement_state " << full_addr << std::endl;
  long distance = rt_position[set] - way;
  rt_position[set] = way ;
  extra_cycle_ = std::abs(distance);
  if (hit)
    hit_cycle[(int)type] += extra_cycle_; 
  else
    miss_cycle[(int)type] += extra_cycle_;


  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void rtlru::replacement_final_stats(){
  std::cout << "\n\nRTM Statistic \n\n";
  std::cout << "LOAD\t\tHIT:\t"       << hit_cycle[(int)access_type::LOAD]        << "\tMISS:\t" << miss_cycle[(int)access_type::LOAD]        << std::endl;
  std::cout << "RFO\t\tHIT:\t"        << hit_cycle[(int)access_type::RFO]         << "\tMISS:\t" << miss_cycle[(int)access_type::RFO]         << std::endl;
  std::cout << "PREFETCH\tHIT:\t"     << hit_cycle[(int)access_type::PREFETCH]    << "\tMISS:\t" << miss_cycle[(int)access_type::PREFETCH]    << std::endl;
  std::cout << "WRITE\t\tHIT:\t"      << hit_cycle[(int)access_type::WRITE]       << "\tMISS:\t" << miss_cycle[(int)access_type::WRITE]       << std::endl;
  std::cout << "TRANSLATION\tHIT:\t"  << hit_cycle[(int)access_type::TRANSLATION] << "\tMISS:\t" << miss_cycle[(int)access_type::TRANSLATION] << std::endl;
}

long rtlru::extra_cycle(){
  return extra_cycle_;

}





