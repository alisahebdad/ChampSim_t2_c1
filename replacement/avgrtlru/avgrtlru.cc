#include "avgrtlru.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>



avgrtlru::avgrtlru(CACHE* cache) : avgrtlru(cache, cache->NUM_SET, cache->NUM_WAY) {
}

avgrtlru::avgrtlru(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  std::cout << "avgrtlru installed \n" ;
  std::cout << "SETS : " << sets << " WAYS : " << ways << std::endl;
  extra_cycle_w = 0;
	this->window_size = 5;
  rt_position = new long[sets]();
  this->track = new std::deque <long int>[sets] ();

  for (int i = 0;i<6;++i){
    hit_cycle[i] = 0ll;
    miss_cycle[i] = 0ll;
  }
}

long avgrtlru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,champsim::address full_addr, access_type type)
{
  //std::cout << "find victim " << full_addr << std::endl;
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);

  //rt_position[set] = result ;
  return std::distance(begin, victim);
}

void avgrtlru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
  
                                 access_type type)
{
  //std::cout << "replacement_cache_fill " <<  full_addr << std::endl;
  // Mark the way as being used on the current cycle
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void avgrtlru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{


  extra_cycle_w = abs(rt_position[set]-way);
  if (this->track[set].size() >= this->window_size)
    this->track[set].pop_front();
  this->track[set].push_back(way);    
  long int sum = 0;
  for (auto it = this->track[set].rbegin(); it != this->track[set].rend(); ++it)
      sum += *it ;

  rt_position[set] = sum/this->track[set].size();



  if (hit)
    hit_cycle[(int)type] += extra_cycle_w; 
  else
    miss_cycle[(int)type] += extra_cycle_w;


  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void avgrtlru::replacement_final_stats(){
  std::cout << "\n\navgrtlru RTM Statistic \n\n";

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

long avgrtlru::extra_cycle(){
	return extra_cycle_w;
}





