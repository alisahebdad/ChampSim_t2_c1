#include "lru_set_interval.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include "cache_entropy.h"

lru_set_interval::lru_set_interval(CACHE* cache) : lru_set_interval(cache, cache->NUM_SET, cache->NUM_WAY) {
  myCache = cache;
}

lru_set_interval::lru_set_interval(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways),NUM_SET(sets), last_used_cycles(static_cast<std::size_t>(sets * ways), 0),last_access(static_cast<std::size_t>(sets),0) {
  myCache = cache;
  short_interval = 0 ;
  interval_sum     = 0 ;
  total_access     = 0 ;
  print_interval_  = 0 ;
  total_interval   = 0 ;
  extra_cycle_holder = 0;
  total_short_interval = 0;
  calc = new CacheEntropyCalculator(sets,ways);
}

long lru_set_interval::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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

void lru_set_interval::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void lru_set_interval::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  // Mark the way as being used on the current cycle
  if (hit && (access_type{type} == access_type::WRITE || access_type{type} == access_type::LOAD ) ){ 
    calc->recordAccess(set,way);
    auto current_cycle_time = (myCache->current_time.time_since_epoch() / myCache->clock_period); 
    auto interval_ = current_cycle_time - last_access[set];
    extra_cycle_holder = static_cast<long>(interval_);
    total_interval += interval_;
    if(interval_ < NUM_WAY ){
      short_interval += 1;
      total_short_interval += interval_;
    }
    interval_sum     += interval_ ;
    total_access     += 1 ;
    if (print_interval_++ == print_interval){
      print_interval_ = 0;
      std::cout << "short interval   #: " << short_interval   << "\n"
                << "total_access     #: " << total_access     << "\n"
                << "Short_Access      : " << ((double)short_interval*100)/(double)total_access << "\n"
                << "Average Access    : " << ((double)total_interval)/(double)total_access << "\n" 
                << "total_interval_avg: " << ((double)total_short_interval)/(double)short_interval
                << "Entropy           : " << (double)calc->computeHnorm() 
                << "\n\n" ;
    }
    last_access[set] = current_cycle_time ;
    
  }
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void lru_set_interval::replacement_final_stats(){
    std::cout << "short interval   #: " << short_interval   << "\n"
              << "total_access     #: " << total_access     << "\n"
              << "Short Access      : " << ((double)short_interval*100)/(double)total_access << "\n";

    std::cout << "short interval   #: " << short_interval   << "\n"
              << "total_access     #: " << total_access     << "\n"
              << "Short_Access      : " << ((double)short_interval*100)/(double)total_access << "\n"
              << "Average Access    : " << ((double)total_interval)/(double)total_access << "\n" 
              << "total_interval_avg: " << ((double)total_short_interval)/(double)short_interval << "\n"
              << "Entropy           : " << (double)calc->computeHnorm()   
              << "\n\n" ;
    for (int i = 0;i<NUM_SET;++i) {
      std::cout << calc->computeSetEntropy(i) << " ";
    }
  std::cout << "\n\n";
}

long lru_set_interval::extra_cycle(){
  long _ = extra_cycle_holder;
  extra_cycle_holder = 0;
  return 0;
}


