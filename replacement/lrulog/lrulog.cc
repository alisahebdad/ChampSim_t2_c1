#include "lrulog.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>

std::ofstream out("access_trace.txt");

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



lrulog::lrulog(CACHE* cache) : lrulog(cache, cache->NUM_SET, cache->NUM_WAY) {}

lrulog::lrulog(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {}

long lrulog::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  auto result = std::distance(begin, victim);
  out << "victim " << set << " " << result << " " << cycle << std::endl;

  //std::cout << "cycle :" << cycle << std::endl;

  return result;
}

void lrulog::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle

  out << "cache_fill " << full_addr << " " <<  set << " " << way << " " << type <<  " " << cycle << std::endl;
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

void lrulog::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  out << "update " << full_addr << " " << set << " " << way << " " << type <<  " " << cycle << std::endl;
  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}


void lrulog::replacement_final_stats(){
  out.close();
  
}
