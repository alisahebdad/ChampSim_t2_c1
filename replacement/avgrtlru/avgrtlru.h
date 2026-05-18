#ifndef REPLACEMENT_AVGRTLRU_H
#define REPLACEMENT_AVGRTLRU_H

#include <vector>
#include <deque>
#include "cache.h"
#include "modules.h"

class avgrtlru : public champsim::modules::replacement
{
  long NUM_WAY;
  std::vector<uint64_t> last_used_cycles;
  uint64_t cycle = 0; 
  long long hit_cycle[6];
  long long miss_cycle[6];
  long *rt_position;
  std::deque<long int> *track;
  size_t window_size ;
  int threshold;
  long extra_cycle_w;

public:
  explicit avgrtlru(CACHE* cache);
  avgrtlru(CACHE* cache, long sets, long ways);

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
