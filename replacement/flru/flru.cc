#include "flru.h"
#include <cstdint>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>
#include "access_type.h"

std::ofstream out_flru("access_trace.txt");

/*
std::ostream& operator<< (std::ostream& os,access_type type){
  switch(type){
    case access_type::LOAD: os << "LOAD";break;
    case access_type::RFO : os << "RFO";break;
    case access_type::PREFETCH: os << "PREFETCH";break;
    case access_type::WRITE: os << "WRITE";break;
    case access_type::TRANSLATION: os << "TRANSLATION";break;
    case access_type::NUM_TYPES: os << "NUM_TYPES";break;
    default: os << "Unknown";break;  
  }
  return os ;
}
*/

flru::flru(CACHE* cache) : flru(cache, cache->NUM_SET, cache->NUM_WAY) {}

flru::flru(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  this->vg_size = 2;
  this->last_rt.resize(sets);
  this->rt_position = new long[sets]();
  for (int i = 0;i<6;++i){
    hit_cycle[i] = 0ll;
    miss_cycle[i] = 0ll;
  }

}

long  flru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  auto result = std::distance(begin, victim);

  std::vector<std::pair<uint64_t,size_t>> _data;
  _data.reserve(NUM_WAY);
  size_t idx = 0;
  for (auto it = begin;it != end ;++it){
     _data.emplace_back(*it,idx++);
  }
  std::sort(_data.begin(), _data.end(),
            [](const auto& a, const auto& b) {
              if (a.first != b.first) return a.first < b.first;
                return a.second < b.second;   // tie‑breaker
            });
  
  size_t minDis = 17;
  size_t candidate = -1;

  for (size_t i = 0 ; i < this->vg_size;++i){
    size_t dis = std::abs(this->rt_position[set] - (long )_data[i].second);
    if (dis < minDis){
      minDis = dis;
      candidate = _data[i].second;
    }
    //std::cout << "(" << _data[i].first << "," << _data[i].second << ")" ;
  
  }



  // std::cout << " " << result <<  " " << candidate <<" " << rt_position[set] <<  std::endl;
   

  out_flru << "victim " << set << " " << result << " " << cycle << std::endl;
  //std::cout << "cycle :" << cycle << std::endl;
  return candidate;
}

void flru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle

  out_flru << "cache_fill " << full_addr << " " <<  set << " " << way << " " << type <<  " " << cycle << std::endl;
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

/*
enum class access_type : unsigned {
  LOAD = 0,
  RFO,
  PREFETCH,
  WRITE,
  TRANSLATION,
  NUM_TYPES,
};

*/

void flru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  out_flru << "update " << full_addr << " " << set << " " << way << " " << type <<  " " << cycle << std::endl;
  // Mark the way as being used on the current cycle
  
  if (way != NUM_WAY){
    //std::cout << "update_replacement_state " << full_addr << " " << this->myCache->NAME << " " << way <<  std::endl;
  
    long distance = rt_position[set] - way;
    rt_position[set] = way ;
    extra_cycle_ = std::abs(distance);
    if (hit)
      hit_cycle[(int)type] += extra_cycle_; 
    else
      miss_cycle[(int)type] += extra_cycle_;
  }

  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
  }


void flru::replacement_final_stats(){
  out_flru.close();
  long long total_hit = 0l,total_miss = 0l;
  for (auto i = 0;i<5;++i){
    total_hit  += hit_cycle[i];
    total_miss += miss_cycle[i]; 
  }

  std::cout << "\n\nRTM Statistic \n\n";
  std::cout << "LOAD\t\tHIT:\t"       << hit_cycle[(int)access_type::LOAD]        << "\tMISS:\t" << miss_cycle[(int)access_type::LOAD]        << std::endl;
  std::cout << "RFO\t\tHIT:\t"        << hit_cycle[(int)access_type::RFO]         << "\tMISS:\t" << miss_cycle[(int)access_type::RFO]         << std::endl;
  std::cout << "PREFETCH\tHIT:\t"     << hit_cycle[(int)access_type::PREFETCH]    << "\tMISS:\t" << miss_cycle[(int)access_type::PREFETCH]    << std::endl;
  std::cout << "WRITE\t\tHIT:\t"      << hit_cycle[(int)access_type::WRITE]       << "\tMISS:\t" << miss_cycle[(int)access_type::WRITE]       << std::endl;
  std::cout << "TRANSLATION\tHIT:\t"  << hit_cycle[(int)access_type::TRANSLATION] << "\tMISS:\t" << miss_cycle[(int)access_type::TRANSLATION] << std::endl;
  std::cout << "TOTAL\tHIT:\t"        << total_hit                                << "\tMISS:\t" << total_miss  << std::endl;


}



long flru::extra_cycle(){
  //std::cout << "extra_cycle " << this->myCache << " " << this->myCache->NAME << std::endl; 
  return extra_cycle_;

}


