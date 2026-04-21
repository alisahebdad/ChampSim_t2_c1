#include "modertlru.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>



modertlru::modertlru(CACHE* cache) : modertlru(cache, cache->NUM_SET, cache->NUM_WAY) {
}

modertlru::modertlru(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  std::cout << "modertlru installed \n" ;
  std::cout << "SETS : " << sets << " WAYS : " << ways << std::endl;
  this->window_size = 8;
  this->threshold = 3;
  std::cout << "window size  : " << this->window_size << " threshold : " << this->threshold <<  std::endl;
  rt_position = new long[sets]();
  //rt_delta = new int[sets]();
  
  this->track = new std::vector <int>[sets] ();
  for (auto j = 0;j<sets;++j)
    for (auto i = 0;i<window_size;++i)
      this->track[j].push_back(0);

  for (int i = 0;i<6;++i){
    hit_cycle[i] = 0ll;
    miss_cycle[i] = 0ll;
  }
}

long modertlru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,champsim::address full_addr, access_type type)
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

void modertlru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
  
                                 access_type type)
{
  //std::cout << "replacement_cache_fill " <<  full_addr << std::endl;
  // Mark the way as being used on the current cycle
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void modertlru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  // mode check : track last window_size input and find mode  
  bool mode_ok = false;
  int freq_count [16] = {0};
  int mode;
  extra_cycle_w = 0;
  for (int u = 0;u<this->window_size;++u)
    if (++freq_count[track[set][u]] >= this->threshold){
      mode_ok = true;
      mode = track[set][u];
    }

  for (int u = 0;u<(this->window_size - 1);++u)
    track[set][u] = track[set][u+1] ;
  track[set][this->window_size-1] = way;

  int distance;
  if (mode_ok) 
    distance = std::abs(way-mode) ;
  else 
    distance = std::abs(way-track[set][this->window_size-2]);
  extra_cycle_w = distance;
  //extra_cycle_w = 10;
  //if (extra_cycle_w<0)
  //std::cout << "Extra : " << extra_cycle_w << " " << mode_ok << std::endl;

  if (hit)
    hit_cycle[(int)type] += extra_cycle_w; 
  else
    miss_cycle[(int)type] += extra_cycle_w;


  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void modertlru::replacement_final_stats(){
  std::cout << "\n\nmodertlru RTM Statistic \n\n";

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

long modertlru::extra_cycle(){
  return extra_cycle_w;
}





