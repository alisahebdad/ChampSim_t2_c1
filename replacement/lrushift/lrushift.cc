#include "lrushift.h"
#include <iostream>
#include <algorithm>
#include <cassert>

lrushift::lrushift(CACHE* cache) : lrushift(cache, cache->NUM_SET, cache->NUM_WAY) {}

lrushift::lrushift(CACHE* cache, long sets, long ways) : replacement(cache),
    history_length(static_cast<std::size_t>(ways * 2)),
    NUM_WAY(ways),
    last_used_cycles(static_cast<std::size_t>(sets * ways), 0),
    history(static_cast<std::size_t>(sets),
            std::deque<long>(history_length, 0)),
    shifts(static_cast<long long>(history_length), 0),
    short_accesses_total(0),
    short_access_count(0),
    accesses_total(0),
    access_count(0),
    myCache(cache),
    last_access(static_cast<std::size_t>(sets), 0),
    add_extra_by(0),
    log_cycle(100000),
    log_cycle_(0)
{}



long lrushift::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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


void lrushift::initialize_replacement(){
  std::cout << "lrushift is installed " << std::endl;
  std::cout << "WAYS :" << NUM_WAY << " len(History[set]) : " << history[0].size() << std::endl;
}

void lrushift::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void lrushift::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  // Mark the way as being used on the current cycle
  if (hit){
    auto current_cycle_time = (myCache->current_time.time_since_epoch() / myCache->clock_period); 
    auto interval_ = current_cycle_time - last_access[set];
    last_access[set] = current_cycle_time;
    log_cycle_++;
    long total = 0;
    for (std::size_t i = 0; i < history[set].size(); ++i) {
      total += history[set][i];
      shifts[i] += std::abs(way - std::lround(static_cast<double>(total) / static_cast<double>(i + 1)));
    }
    if (interval_ < static_cast<uint64_t> (NUM_WAY) ){
      short_accesses_total += interval_;
      short_access_count++;
    }
    accesses_total += interval_;
    access_count += 1;
    //if (std::abs(history[set]-way))
    if (log_cycle_ == log_cycle){
      // csv write 
      this->appendToCSV("stat.csv",short_accesses_total,short_access_count,accesses_total,access_count,shifts);
      std::fill(shifts.begin(),shifts.end(),0);
      log_cycle_ = 0;
      short_access_count   = 0;
      short_accesses_total = 0;
      accesses_total = 0;
      access_count   = 0;
    }
   

  
    history[set].push_front(way);
    history[set].pop_back();
 
  
  
  }
  // update history of access for set 
 if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}


void lrushift::replacement_final_stats(){
 
}
long lrushift::extra_cycle(){
  return 0;
}
void lrushift::appendToCSV(const std::string& filename,
                 size_t value1,
                 size_t value2,
                 size_t value3,
                 size_t value4,
                 const std::vector<long long>& data)
{
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open())
        throw std::runtime_error("Cannot open CSV file.");
    // Write the two size_t values
    file << value1 << "," << value2 << "," << value3 << "," << value4 ;
    // Write all vector elements
    for (long long x : data)
        file << "," << x;
    file << '\n';
}


