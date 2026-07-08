#ifndef REPLACEMENT_LRU_HW_H
#define REPLACEMENT_LRU_HW_H

#include <cstdint>
#include <vector>

#include "cache.h"
#include "modules.h"

// lru_hw: plain LRU replacement that additionally keeps, per set, a "predicted"
// victim way (the way that was evicted last time in that set). On every
// find_victim we compare the prediction against the real LRU victim and count
// a mispredict when they differ. The running total is printed at the end of
// the simulation.
struct lru_hw : champsim::modules::replacement {
  long NUM_SET;
  long NUM_WAY;

  // Recency stamps, one per (set, way).
  std::vector<uint64_t> last_used_cycles;
  uint64_t cycle = 0;

  // Predicted victim way for each set.
  std::vector<long> predicted_way;

  // Sum of mispredictions over the whole run.
  uint64_t mispredict_count = 0;

  explicit lru_hw(CACHE* cache);
  lru_hw(CACHE* cache, long sets, long ways);

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);

  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,access_type type);


  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                champsim::address victim_addr, access_type type, uint8_t hit);

  void replacement_final_stats();
};

#endif
