#include "unirtlru.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Create CSV file and write header
void init_csv_file(const std::string& filename,
                   const std::vector<std::string>& headers)
{
    std::ofstream file(filename);

    for (size_t i = 0; i < headers.size(); ++i)
    {
        if (i > 0)
            file << ',';
        file << headers[i];
    }
    file << '\n';
}

// Append one row of long long values
void add_csv_row(const std::string& filename,
                 const std::vector<long long>& row)
{
    std::ofstream file(filename, std::ios::app);

    for (size_t i = 0; i < row.size(); ++i)
    {
        if (i > 0)
            file << ',';
        file << row[i];
    }
    file << '\n';
}

unirtlru::unirtlru(CACHE* cache) : unirtlru(cache, cache->NUM_SET, cache->NUM_WAY) {}

unirtlru::unirtlru(CACHE* cache, long sets, long ways) : replacement(cache), NUM_WAY(ways), last_used_cycles(static_cast<std::size_t>(sets * ways), 0) {

  access_sq = new long*[sets];
  benefit = new bool[sets];
  next_save = save_step;
  for (int i = 0; i < sets; i++){
    access_sq[i] = new long[seq_num];
    for (int w = 0; w < seq_num;++w)
      access_sq[i][w] = ways;
  }

  fs::create_directories(fs::path("output"));
  for (int i = 1;i<10;++i) { 
    init_csv_file("output/avg"+ std::to_string(i)+".csv",{
      "hit_LOAD"       , "miss_LOAD", 
      "hit_RFO"        , "miss_RFO",
      "hit_PREFETCH"   , "miss_PREFETCH",
      "hit_WRITE"      , "miss_WRITE",
      "hit_TRANSLATION", "miss_TRANSLATION",
      "hit_NUM_TYPES"  , "miss_NUM_TYPES",
      "total_access"
    });
    init_csv_file("output/histogram_"+ std::to_string(i)+".csv",{
      "0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15"
    });
  init_csv_file("output/avg_nmru"+ std::to_string(i)+".csv",{
      "hit_LOAD"       , "miss_LOAD", 
      "hit_RFO"        , "miss_RFO",
      "hit_PREFETCH"   , "miss_PREFETCH",
      "hit_WRITE"      , "miss_WRITE",
      "hit_TRANSLATION", "miss_TRANSLATION",
      "hit_NUM_TYPES"  , "miss_NUM_TYPES",
      "total_access"
    });
  init_csv_file("output/avg_nmru_I1"+ std::to_string(i)+".csv",{
      "hit_LOAD"       , "miss_LOAD", 
      "hit_RFO"        , "miss_RFO",
      "hit_PREFETCH"   , "miss_PREFETCH",
      "hit_WRITE"      , "miss_WRITE",
      "hit_TRANSLATION", "miss_TRANSLATION",
      "hit_NUM_TYPES"  , "miss_NUM_TYPES",
      "total_access"
    });


  }
    
  clean_data();

}

long unirtlru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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

void unirtlru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void unirtlru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{

  if (next_save < cycle){
    next_save += save_step;
    save_data();
    clean_data();
  }
  
  for (int w = seq_num - 1; w > 0 ;--w)
    access_sq[set][w] = access_sq[set][w-1];
  access_sq[set][0] = way;
  cal_avg(set,type,hit);
  cal_avg_nmru(set,type,hit);
  cal_avg_nmru_indicate1(set,type,hit);
  
  
  // Mark the way as being used on the current cycle
  if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}



long unirtlru::extra_cycle(){
  return 0;
}

void unirtlru::save_data(){
  // std::cout << "Total Hit " << avg[2].total_access << std::endl;
  // for (int avgX = 1;avgX < 10;avgX++){
  //   for (int j = 0 ;j<10;++j)
  //     std::cout << avgX << " " << avg[avgX].hit[j] << " " << avg[avgX].miss[j] << std::endl; 
  // }
    for (int i = 1;i<10;++i){  

      add_csv_row("output/histogram_"+ std::to_string(i)+".csv",{
        hist[i][0],hist[i][1],hist[i][2],hist[i][3],
        hist[i][4],hist[i][5],hist[i][6],hist[i][7],
        hist[i][8],hist[i][9],hist[i][10],hist[i][11],
        hist[i][12],hist[i][13],hist[i][14],hist[i][15],
      });
      
      add_csv_row("output/avg"+ std::to_string(i)+".csv",
      {
        avg[i].hit[0],avg[i].miss[0],
        avg[i].hit[1],avg[i].miss[1],
        avg[i].hit[2],avg[i].miss[2],
        avg[i].hit[3],avg[i].miss[3],
        avg[i].hit[4],avg[i].miss[4],
        avg[i].hit[5],avg[i].miss[5],
        avg[i].total_access
      });

      add_csv_row("output/avg_nmru"+ std::to_string(i)+".csv",
      {
        avg_nmru[i].hit[0],avg_nmru[i].miss[0],
        avg_nmru[i].hit[1],avg_nmru[i].miss[1],
        avg_nmru[i].hit[2],avg_nmru[i].miss[2],
        avg_nmru[i].hit[3],avg_nmru[i].miss[3],
        avg_nmru[i].hit[4],avg_nmru[i].miss[4],
        avg_nmru[i].hit[5],avg_nmru[i].miss[5],
        avg_nmru[i].total_access
      });
      
        add_csv_row("output/avg_nmru_I1"+ std::to_string(i)+".csv",
      {
        avg_nmru_i1[i].hit[0],avg_nmru_i1[i].miss[0],
        avg_nmru_i1[i].hit[1],avg_nmru_i1[i].miss[1],
        avg_nmru_i1[i].hit[2],avg_nmru_i1[i].miss[2],
        avg_nmru_i1[i].hit[3],avg_nmru_i1[i].miss[3],
        avg_nmru_i1[i].hit[4],avg_nmru_i1[i].miss[4],
        avg_nmru_i1[i].hit[5],avg_nmru_i1[i].miss[5],
        avg_nmru_i1[i].total_access
      });
    
    }
}

void unirtlru::cal_avg(long set,access_type type, uint8_t hit){
  long current_way = access_sq[set][0];
  hist[0][current_way]+=1;
  long sum = 0;
  for (int avgX = 1;avgX < 10;avgX++){
    sum += access_sq[set][avgX];
    hist[avgX][std::abs((sum/avgX)-current_way)] += 1; 
    if (hit)
      avg[avgX].hit[ static_cast<int>(type) ]  += std::abs((sum/avgX)-current_way);
    else{
        avg[avgX].miss[ static_cast<int>(type) ] += std::abs((sum/avgX)-current_way);
      }
    avg[avgX].total_access += 1;
    }
  
}

void unirtlru::cal_avg_nmru(long set,access_type type, uint8_t hit){
  long current_way = access_sq[set][0];
  long sum = 0;
  for (int avgX = 2;avgX < 10;avgX++){
    sum += access_sq[set][avgX];
    if (hit)
      avg_nmru[avgX].hit[ static_cast<int>(type) ]  += std::abs((sum/avgX)-current_way);
    else
      avg_nmru[avgX].miss[ static_cast<int>(type) ] += std::abs((sum/avgX)-current_way);
      avg_nmru[avgX].total_access += 1;
    }
}

void unirtlru::clean_data(){
  for (int i = 0; i<10; ++i){
    for (int j = 0 ; j < 16;++j)
      hist[i][j] = 0 ;
    for(int j = 0;j<10;++j)
    {
      avg[i].hit[j]  = 0;
      avg[i].miss[j] = 0;
      avg_nmru[i].hit[j] = 0;
      avg_nmru[i].miss[j] = 0;
      avg_nmru_i1[i].hit[j] = 0;
      avg_nmru_i1[i].miss[j] = 0;
    }  
    avg[i].total_access = 0;
  }


}

void unirtlru::cal_avg_nmru_indicate1(long set,access_type type, uint8_t hit){
  long current_way = access_sq[set][0];
  long sum = 0;
  long long no_shift = std::abs(access_sq[set][0]-access_sq[set][1]);
  for (int avgX = 2;avgX < 10;avgX++){
    sum += access_sq[set][avgX];
    long long shift = std::abs((sum/avgX)-current_way);

    if (hit)
      avg_nmru_i1[avgX].hit[ static_cast<int>(type) ]  += benefit[set] ?  shift : no_shift;
    else
      avg_nmru_i1[avgX].miss[ static_cast<int>(type) ] += benefit[set] ?  shift : no_shift;
    
    if (shift > no_shift)
      benefit[set] = true;
    else
      benefit[set] = false;
    avg_nmru_i1[avgX].total_access += 1;
    }
}