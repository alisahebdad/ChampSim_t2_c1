#ifndef REPLACEMENT_LRUSHIFT_H
#define REPLACEMENT_LRUSHIFT_H

#include <vector>

#include "cache.h"
#include "modules.h"
#include <deque>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>

class lrushift : public champsim::modules::replacement
{
  const size_t history_length;
  long NUM_WAY;
  std::vector<uint64_t> last_used_cycles;
  uint64_t cycle = 0;
  std::vector<std::deque<long>> history;   // one deque per set
  std::vector<long long> shifts;
  size_t short_accesses_total,short_access_count;
  size_t accesses_total,access_count;
  CACHE *myCache; 
  std::vector<uint64_t> last_access;
  size_t add_extra_by;
  long extra_cycle_;
  size_t log_cycle,log_cycle_; 
   
public:
  explicit lrushift(CACHE* cache);
  lrushift(CACHE* cache, long sets, long ways);

  void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);
  void replacement_final_stats();
  void appendToCSV(const std::string& filename,
                 size_t value1,
                 size_t value2,
                 size_t value3,
                 size_t value4,
                 const std::vector<long long>& data);

  long extra_cycle();

};

#endif
