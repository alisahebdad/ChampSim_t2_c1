#ifndef REPLACEMENT_LRUHISTOGRAM_H
#define REPLACEMENT_LRUHISTOGRAM_H

#include <vector>

#include "cache.h"
#include "modules.h"
#include "csv_manager.h"


class lruhistogram : public champsim::modules::replacement
{
  long NUM_WAY;
  std::vector<uint64_t> last_used_cycles;
  long set_rtm_last7_positions[4096][7];
  long long histogram_r[16];
  long long histogram_w[16];
  long long histogram_r_avg5[16];
  long long histogram_w_avg5[16];
  long long histogram_r_avg6[16];
  long long histogram_w_avg6[16];
  CsvWriter *csv,*csv_avg5,*csv_avg6;
  long long write_period ;
  const long long write_period_cycle = 10000;
  uint64_t cycle = 0;

  long long cal_lazy_mode(long set);
  long long cal_avg5_mode(long set);
  long long cal_avg6_mode(long set);



public:
  explicit lruhistogram(CACHE* cache);
  lruhistogram(CACHE* cache, long sets, long ways);

  // void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);
  void replacement_final_stats();


};

#endif
