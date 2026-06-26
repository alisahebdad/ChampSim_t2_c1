#include "lruhistogram.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <cmath>

lruhistogram::lruhistogram(CACHE* cache) : lruhistogram(cache, cache->NUM_SET, cache->NUM_WAY) {}

lruhistogram::lruhistogram(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  std::cout << "LRU Histogram install " << std::endl;
  this->csv = new CsvWriter("histogram.csv");
  this->csv_avg5 = new CsvWriter("histogram_avg5.csv");
  this->csv_avg6 = new CsvWriter("histogram_avg6.csv");

  this->csv->addHeader({
      "0_r","0_w","1_r","1_w","2_r","2_w","3_r","3_w","4_r","4_w","5_r","5_w","6_r","6_w","7_r","7_w","8_r",
      "8_w","9_r","9_w","10_r","10_w","11_r","11_w","12_r","12_w","13_r","13_w","14_r","14_w","15_r","15_w"});
  
  this->csv_avg5->addHeader({
      "0_r","0_w","1_r","1_w","2_r","2_w","3_r","3_w","4_r","4_w","5_r","5_w","6_r","6_w","7_r","7_w","8_r",
      "8_w","9_r","9_w","10_r","10_w","11_r","11_w","12_r","12_w","13_r","13_w","14_r","14_w","15_r","15_w"});
  
  this->csv_avg6->addHeader({
      "0_r","0_w","1_r","1_w","2_r","2_w","3_r","3_w","4_r","4_w","5_r","5_w","6_r","6_w","7_r","7_w","8_r",
      "8_w","9_r","9_w","10_r","10_w","11_r","11_w","12_r","12_w","13_r","13_w","14_r","14_w","15_r","15_w"});

  // initialize the rtm related array 
  for (size_t idx = 0;idx < 4096;++idx)
    for (size_t ord = 0;ord<7;ord++)
      set_rtm_last7_positions[idx][ord] = ways-1;
  for (size_t idx = 0;idx<16;++idx){
    histogram_w[idx] = 0;
    histogram_r[idx] = 0;
    histogram_r_avg5[idx] = 0;
    histogram_w_avg5[idx] = 0;
    histogram_r_avg6[idx] = 0;
    histogram_w_avg6[idx] = 0;
 
  }

  write_period = write_period_cycle;


}

long lruhistogram::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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

void lruhistogram::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle

  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

long long lruhistogram::cal_lazy_mode(long set){
  return std::abs(set_rtm_last7_positions[set][0] - set_rtm_last7_positions[set][1]);
}

long long lruhistogram::cal_avg5_mode(long set){
  long long sum = 0;
  for (int i = 1;i<6;++i)
    sum += set_rtm_last7_positions[set][i];
  double avg = std::round(static_cast<double>(sum) / 5.0);
  return static_cast<long long>(
    std::abs(avg - static_cast<double>(set_rtm_last7_positions[set][0])));
}

long long lruhistogram::cal_avg6_mode(long set){
  long long sum = 0;
  for (int i = 1;i<7;++i)
    sum += set_rtm_last7_positions[set][i];
  double avg = std::round(static_cast<double>(sum) / 6.0);
  //if (set_rtm_last7_positions[set][1] != static_cast<long long>(avg))
  //  std::cout << set_rtm_last7_positions[set][1] << " " << avg << std::endl;
  /*if (set == 10){
    for (int i = 0;i<7;++i,std::cout << " " )
      std::cout << set_rtm_last7_positions[set][i];
    std:: cout << " AVG " << avg << std::endl;
  }
  */
  return static_cast<long long>(
    std::abs(avg - static_cast<double>(set_rtm_last7_positions[set][0])));
}

void lruhistogram::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
  if (way == NUM_WAY)
    return ;

  for (int idx = 5;idx>=0;--idx){
    //std::cout << idx << std::endl;
    set_rtm_last7_positions[set][idx+1] = set_rtm_last7_positions[set][idx];
  }
    set_rtm_last7_positions[set][0] = way;
  if (type == access_type::WRITE){
    histogram_w[cal_lazy_mode(set)] += 1;
    histogram_w_avg5[cal_avg5_mode(set)] += 1;
    histogram_w_avg6[cal_avg6_mode(set)] += 1;
  }else{
    histogram_r[cal_lazy_mode(set)] += 1; 
    histogram_r_avg5[cal_avg5_mode(set)] += 1;
    histogram_r_avg6[cal_avg6_mode(set)] += 1;
  }

  if (write_period--<0){
    write_period = write_period_cycle;
    csv->appendRow({
        histogram_r[0],histogram_w[0],histogram_r[1],histogram_w[1],
        histogram_r[2],histogram_w[2],histogram_r[3],histogram_w[3],
        histogram_r[4],histogram_w[4],histogram_r[5],histogram_w[5],
        histogram_r[6],histogram_w[6],histogram_r[7],histogram_w[7],
        histogram_r[8],histogram_w[8],histogram_r[9],histogram_w[9],
        histogram_r[10],histogram_w[10],histogram_r[11],histogram_w[11],
        histogram_r[12],histogram_w[12],histogram_r[13],histogram_w[13],
        histogram_r[14],histogram_w[14],histogram_r[15],histogram_w[15]
        });
    csv_avg5->appendRow({
        histogram_r_avg5[0],histogram_w_avg5[0],histogram_r_avg5[1],histogram_w_avg5[1],
        histogram_r_avg5[2],histogram_w_avg5[2],histogram_r_avg5[3],histogram_w_avg5[3],
        histogram_r_avg5[4],histogram_w_avg5[4],histogram_r_avg5[5],histogram_w_avg5[5],
        histogram_r_avg5[6],histogram_w_avg5[6],histogram_r_avg5[7],histogram_w_avg5[7],
        histogram_r_avg5[8],histogram_w_avg5[8],histogram_r_avg5[9],histogram_w_avg5[9],
        histogram_r_avg5[10],histogram_w_avg5[10],histogram_r_avg5[11],histogram_w_avg5[11],
        histogram_r_avg5[12],histogram_w_avg5[12],histogram_r_avg5[13],histogram_w_avg5[13],
        histogram_r_avg5[14],histogram_w_avg5[14],histogram_r_avg5[15],histogram_w_avg5[15]
        });
    csv_avg6->appendRow({
        histogram_r_avg6[0],histogram_w_avg6[0],histogram_r_avg6[1],histogram_w_avg6[1],
        histogram_r_avg6[2],histogram_w_avg6[2],histogram_r_avg6[3],histogram_w_avg6[3],
        histogram_r_avg6[4],histogram_w_avg6[4],histogram_r_avg6[5],histogram_w_avg6[5],
        histogram_r_avg6[6],histogram_w_avg6[6],histogram_r_avg6[7],histogram_w_avg6[7],
        histogram_r_avg6[8],histogram_w_avg6[8],histogram_r_avg6[9],histogram_w_avg6[9],
        histogram_r_avg6[10],histogram_w_avg6[10],histogram_r_avg6[11],histogram_w_avg6[11],
        histogram_r_avg6[12],histogram_w_avg6[12],histogram_r_avg6[13],histogram_w_avg6[13],
        histogram_r_avg6[14],histogram_w_avg6[14],histogram_r_avg6[15],histogram_w_avg6[15]
        });
    for (int i = 0 ;i<16;++i){
      histogram_w[i] = 0;
      histogram_r[i] = 0;
      histogram_w_avg5[i] = 0;
      histogram_r_avg5[i] = 0;
      histogram_w_avg6[i] = 0;
      histogram_r_avg6[i] = 0;
    }

  }

}

void lruhistogram::replacement_final_stats(){
  std::cout << "LRU Histogram is done " << std::endl;
  this->csv->save();
  this->csv_avg5->save();
  this->csv_avg6->save();
}





