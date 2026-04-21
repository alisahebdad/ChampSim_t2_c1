#include "deltartlru.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string.h>

long extra_cycle_w;


std::string access_type_to_string(uint8_t type) {
    switch (type) {
      case (uint8_t)access_type::LOAD:        return "LOAD";
      case (uint8_t)access_type::RFO:         return "RFO";
      case (uint8_t)access_type::PREFETCH:    return "PREFETCH";
      case (uint8_t)access_type::WRITE:       return "WRITE";
      case (uint8_t)access_type::TRANSLATION: return "TRANSLATION";
      case (uint8_t)access_type::NUM_TYPES:   return "NUM_TYPES";
      default:                                return "UNKNOWN";
    }
}


deltartlru::deltartlru(CACHE* cache) : deltartlru(cache, cache->NUM_SET, cache->NUM_WAY) {
}

deltartlru::deltartlru(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  std::cout << "deltartlru installed \n" ;
  this->ignore_prefetch = true;
  std::cout << "SETS : " << sets << " WAYS : " << ways << std::endl;
  std::cout << "Ignore for prefetcher : " << this->ignore_prefetch << std::endl;
  this->window_size = 4;
  std::cout << "Number of track : " << this->window_size << std::endl;
  rt_position = new long[sets]();
  //rt_delta = new int[sets]();
  
  this->track = new std::vector <long>[sets] ();
  for (auto j = 0;j<sets;++j)
    for (auto i = 0;i<this->window_size;++i)
      this->track[j].push_back(0);



  for (int i = 0;i<6;++i){
    hit_cycle[i] = 0ll;
    miss_cycle[i] = 0ll;
  }
}

long deltartlru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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

void deltartlru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  //std::cout << "replacement_cache_fill " <<  full_addr << std::endl;
  // Mark the way as being used on the current cycle
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void deltartlru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  // stride check 
  bool stride_ok = false;
  //std::cout << access_type_to_string((uint8_t)type) << std::endl;
  if (this->ignore_prefetch == true && 
      (type == access_type::WRITE || type==access_type::LOAD))
  {
    if (track[set][0] == track[set][2] && 
      track[set][1] == track[set][3])
    stride_ok = true;
    // add to quque 
    track[set][3] = track[set][2];
    track[set][2] = track[set][1];
    track[set][1] = track[set][0]; 
    track[set][0] = way;

  }
  long distance = stride_ok ? 0 :  std::abs(way-rt_position[set]); //stride_ok ? 0 : (track[set][1] - track[set][0]) ;
  rt_position[set] = way;

  extra_cycle_w = distance;
  if (hit)
    hit_cycle[(int)type] += extra_cycle_w; 
  else
    miss_cycle[(int)type] += extra_cycle_w;


  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void deltartlru::replacement_final_stats(){

  std::cout << "\n\ndeltartlru RTM Statistic \n\n";

  long long total_hit = 0l,total_miss = 0l;
  for (auto i = 0;i<5;++i){
    total_hit  += hit_cycle[i];
    total_miss += miss_cycle[i]; 
  }

  std::cout << "LOAD\t\tHIT:\t"       << hit_cycle[(int)access_type::LOAD]        << "\tMISS:\t" << miss_cycle[(int)access_type::LOAD]        << std::endl;
  std::cout << "RFO\t\tHIT:\t"        << hit_cycle[(int)access_type::RFO]         << "\tMISS:\t" << miss_cycle[(int)access_type::RFO]         << std::endl;
  std::cout << "PREFETCH\tHIT:\t"     << hit_cycle[(int)access_type::PREFETCH]    << "\tMISS:\t" << miss_cycle[(int)access_type::PREFETCH]    << std::endl;
  std::cout << "WRITE\t\tHIT:\t"      << hit_cycle[(int)access_type::WRITE]       << "\tMISS:\t" << miss_cycle[(int)access_type::WRITE]       << std::endl;
  std::cout << "TRANSLATION\tHIT:\t"  << hit_cycle[(int)access_type::TRANSLATION] << "\tMISS:\t" << miss_cycle[(int)access_type::TRANSLATION] << std::endl;
  std::cout << "TOTAL\tHIT:\t"        << total_hit                                << "\tMISS:\t" << total_miss  << std::endl;

}

long deltartlru::extra_cycle(){
  return extra_cycle_w;

}





