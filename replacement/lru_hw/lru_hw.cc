#include "lru_hw.h"

#include <algorithm>
#include <cassert>
#include <iostream>

lru_hw::lru_hw(CACHE* cache) : lru_hw(cache, cache->NUM_SET, cache->NUM_WAY) {}

lru_hw::lru_hw(CACHE* cache, long sets, long ways)
    : replacement(cache), NUM_SET(sets), NUM_WAY(ways),
      last_used_cycles(static_cast<std::size_t>(sets * ways), 0),
      predicted_way(static_cast<std::size_t>(sets), 0)
{
}

long lru_hw::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                         champsim::address full_addr, access_type type)
{
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim);
}

void lru_hw::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}


void lru_hw::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,champsim::address victim_addr, access_type type, uint8_t hit)
{
  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;

  mispredict_count += std::abs(predicted_way.at(static_cast<std::size_t>(set))-way); 
  predicted_way.at(static_cast<std::size_t>(set)) = way; // predict here 
  // for default it predect the last access 
}

void lru_hw::replacement_final_stats()
{
  std::cout << "lru_hw: total way mispredicts = " << mispredict_count << std::endl;
}
