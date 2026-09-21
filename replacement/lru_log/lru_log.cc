#include "lru_log.h"

#include <algorithm>
#include <cassert>

lru_log::lru_log(CACHE* cache) : lru_log(cache, cache->NUM_SET, cache->NUM_WAY) {}

lru_log::lru_log(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  logger = new SimulationLogger("/home/ownergive/Documents/result");  
  myCache = cache;

}

long lru_log::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  auto current_cycle_time = (myCache->current_time.time_since_epoch() / myCache->clock_period); 
  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  logger->log_find_victim(
        current_cycle_time,
        triggering_cpu,
        instr_id,
        set,
        ip,
        full_addr,
        type
    );
  return std::distance(begin, victim);
}

void lru_log::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{

  auto current_cycle_time = (myCache->current_time.time_since_epoch() / myCache->clock_period); 
  logger->log_replacement_cache_fill(
        current_cycle_time,
        triggering_cpu,
        set,
        way,
        full_addr,
        ip,
        victim_addr,
        type
    );



  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void lru_log::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  // Mark the way as being used on the current cycle
  //

  auto current_cycle_time = (myCache->current_time.time_since_epoch() / myCache->clock_period); 

  logger->log_update_replacement_state(
    current_cycle_time,
    triggering_cpu,
    set,
    way,
    full_addr,
    ip,
    victim_addr,
    type,
    hit
  );


  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}


void lru_log::replacement_final_stats(){
  delete (this->logger);  
}
