#ifndef REPLACEMENT_LRU_SET_INTERVAL_H
#define REPLACEMENT_LRU_SET_INTERVAL_H

#include <vector>

#include "cache.h"
#include "modules.h"
#include "cache_entropy.h"
class lru_set_interval : public champsim::modules::replacement
{
  long NUM_WAY;
  long NUM_SET;
  CACHE *myCache;
  std::vector<uint64_t> last_used_cycles;
  std::vector<uint64_t> last_access;

  uint64_t short_interval ;
  uint64_t interval_sum ;
  uint64_t total_access;
  uint64_t total_interval;
  uint64_t total_short_interval;
  int print_interval_ ;
  const int print_interval = 10000;
  long extra_cycle_holder; 

  uint64_t cycle = 0;
  CacheEntropyCalculator *calc; 
public:
  explicit lru_set_interval(CACHE* cache);
  lru_set_interval(CACHE* cache, long sets, long ways);

  // void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);
  void replacement_final_stats();

  long extra_cycle();
};

#endif
