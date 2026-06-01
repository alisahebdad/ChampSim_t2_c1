#ifndef REPLACEMENT_UNILRU_H
#define REPLACEMENT_UNILRU_H

#include <vector>

#include "cache.h"
#include "modules.h"

struct instance{
  long long hit[10];
  long long miss[10];
  bool last ;
  long long total_access;
};


class unirtlru : public champsim::modules::replacement
{
  long NUM_WAY;
  std::vector<uint64_t> last_used_cycles;
  uint64_t cycle = 0;
  long **access_sq;
  bool *benefit;
  uint64_t next_save ;
  struct instance avg[10];
  struct instance avg_nmru[10];
  struct instance avg_nmru_i1[10];

public:
  explicit unirtlru(CACHE* cache);
  unirtlru(CACHE* cache, long sets, long ways);
  const int seq_num = 10;
  const int save_step = 1000*100;


  // void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);
  // void replacement_final_stats()

  void save_data();

  void cal_avg(long set,access_type type, uint8_t hit);

  void cal_avg_nmru(long set,access_type type, uint8_t hit);

  void cal_avg_nmru_indicate1(long set,access_type type, uint8_t hit);

  void clean_data();

  long extra_cycle();


};

#endif
