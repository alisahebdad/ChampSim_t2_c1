#include "avglru2.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <cmath>
#include <string>

avglru2::avglru2(CACHE* cache) : avglru2(cache, cache->NUM_SET, cache->NUM_WAY) {}

avglru2::avglru2(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {
  std::cout << "AVG5LRU Histogram install " << std::endl;
  extra_cycle_var = 0;
  this->csv = new CsvWriter("DBC_intervalo.csv");
  std::vector<std::string> labels;
  for (size_t idx = 0;idx<200;++idx){
    labels.push_back(std::to_string(idx));   
  }
  this->csv->addHeader(labels);
  this->intervals.assign(this->latency_domain,0LL);

 // initialize the rtm related array
  for (size_t idx = 0;idx < 4096;++idx){
    last_cycle[idx] = 0;
    for (size_t ord = 0;ord<7;ord++)
      set_rtm_last7_positions[idx][ord] = ways-1;
  }
  write_period = write_period_cycle;


}

long avglru2::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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

void avglru2::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle

  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

long long avglru2::cal_lazy_mode(long set){
  return std::abs(set_rtm_last7_positions[set][0] - set_rtm_last7_positions[set][1]);
}

long long avglru2::cal_avg5_mode(long set){
  long long sum = 0;
  for (int i = 1;i<6;++i)
    sum += set_rtm_last7_positions[set][i];
  double avg = std::round(static_cast<double>(sum) / 5.0);
  return static_cast<long long>(
    std::abs(avg - static_cast<double>(set_rtm_last7_positions[set][0])));
}

long long avglru2::cal_avg6_mode(long set){
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

void avglru2::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  // Mark the way as being used on the current cycle
  
  auto cycle_ = intern_->current_time.time_since_epoch() / intern_->clock_period;
  
  std::cout << "cycle : " << cycle_ << std::endl;

  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
  if (way == NUM_WAY)
    return ;
  if ( ( (cycle-last_cycle[set])/10 ) < this->latency_domain )
    intervals[((cycle-last_cycle[set]) /10)] += 1;
  else
    intervals[(this->latency_domain-1)] += 1;
  last_cycle[set] = cycle;
  for (int idx = 5;idx>=0;--idx){
    //std::cout << idx << std::endl;
    set_rtm_last7_positions[set][idx+1] = set_rtm_last7_positions[set][idx];
  }
    set_rtm_last7_positions[set][0] = way;



  if (write_period--<0){
    write_period = write_period_cycle;
    csv->appendRow(intervals);
    intervals.assign(this->latency_domain,0LL);

  }
  extra_cycle_var = cal_avg5_mode(set);
}

long avglru2::extra_cycle(){

  return extra_cycle_var;

}

void avglru2::replacement_final_stats(){
  std::cout << "AVG5LRU count cycle interval is done " << std::endl;
  this->csv->save();
}
