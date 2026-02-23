#include "cache.h"
#include "c1.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <cassert>
#include "memory_trace.h"


uint64_t c1::byte2region(uint64_t addr){
  return addr >> 10;
}


int c1::search_rm(uint64_t addr){   //  return location of address region if exist either return -1 
  const uint64_t rm_tag = c1::byte2region(addr);
  for (std::size_t idx{0};idx<RM_ENTRIES;++idx){
    if (RM[idx].age != -1){
      if(RM[idx].region == rm_tag)
        return idx;
    }
  }
  return -1;
}


c1::RM_entry c1::insert_rm_entry(uint64_t addr){
  const uint64_t rm_tag = c1::byte2region(addr);
  size_t oldIdx = -1;
  int oldAge = -1;
  RM_entry result ;
  for (std::size_t idx{0};idx < RM_ENTRIES;++idx) //  increase all age beacuse new entry will insert 
    if (RM[idx].age != -1 )
      RM[idx].age++;

  for (std::size_t idx{0};idx < RM_ENTRIES;++idx){
    if (RM[idx].age < 0){
      oldIdx = idx;
      break;
    }else{
      if (oldAge < RM[idx].age){
        oldAge = RM[idx].age;
        oldIdx = idx;
      }
    }
  }
  
  result = RM[oldIdx]; 

  c1::reset_rm_entry(oldIdx);
  RM[oldIdx].region = rm_tag;
  RM[oldIdx].age = 0;
  return result;
}

void c1::print_RM(){
  std::cout << "-----------------RM-----------------\n";
  std::cout << "id\tage\tregion\tcache\t\t\tpc\n";
  for (std::size_t i{0} ; i<c1::RM_ENTRIES;++i){
    std::cout  << std::dec << i << "\t" <<std::dec << c1::RM[i].age << "\t" << std::hex << c1::RM[i].region <<"\t";
    for (int w = c1::REGION_SIZE-1;w>=0;--w)
      std::cout << c1::RM[i].cache_line_bit_vector[w];
    std::cout << "\t";
    for (int w = c1::IM_ENTRIES-1;w>=0;--w)
      std::cout << c1::RM[i].pc_bit_vector[w];
    std::cout << std::endl;
  }
}


void c1::reset_rm_entry(std::size_t idx){
  assert(idx < RM_ENTRIES);
  if (idx >= RM_ENTRIES)
    return ;

  c1::RM[idx].age = -1;
  std::fill(c1::RM[idx].cache_line_bit_vector,
            c1::RM[idx].cache_line_bit_vector+c1::REGION_SIZE,
            false);
  std::fill(c1::RM[idx].pc_bit_vector,
            c1::RM[idx].pc_bit_vector+c1::REGION_SIZE,
            false); 
  c1::RM[idx].age = -1;
 
}


void CACHE::prefetcher_initialize() {
  for (std::size_t idx{0} ;idx < c1::RM_ENTRIES;++idx ){
    c1::reset_rm_entry(idx);
  }
  for (std::size_t idx{0};idx <c1::IM_ENTRIES;++idx)
    c1::IM[idx].valid = false;
  

}

void c1::update_RM_age(uint64_t addr){
  auto idx = c1::search_rm(addr);
  if (idx != -1 ){
    for (std::size_t i = 0;i<RM_ENTRIES;++i)
      if (c1::RM[i].age != -1 && c1::RM[i].age < c1::RM[idx].age)
        c1::RM[i].age++;
    c1::RM[idx].age = 0;
  }
}

void c1::set_cache_line(uint64_t addr){
  auto find = c1::search_rm(addr);
  if (find != -1){
    int cache_line = (addr >> 6) & 0b1111; 
    RM[find].cache_line_bit_vector[cache_line] = 1;
  }


}



int c1::insert_IM_instruction(uint64_t ip){
  if (!(c1::full_IM())){
    for (std::size_t x{0};x<IM_ENTRIES;x++){
      if (!IM[x].valid){
        IM[x].valid = true;
        IM[x].pc = ip ;
        IM[x].denseR = 0 ;
        IM[x].totalR = 0 ;
        IM[x].age = IM_MAX_AGE;
        im_size++;
        return x;
      }
    }    
  }
  return -1;
}
void c1::evict_IM_instruction(uint64_t ip){

}
int  c1::find_IM_instruction(uint64_t ip){
  for (std::size_t x{0};x<IM_ENTRIES;++x)
    if (IM[x].valid)
      if (IM[x].pc == ip){
        IM[x].age = c1::IM_MAX_AGE;
        return x;
      }
  return -1;
}
bool c1::full_IM(){
  return IM_ENTRIES == (im_size) ;
}

void c1::print_IM(){
  std::cout << "-------------------IM--------------------\n";
  std::cout << "Valid\t" << "PC\t" << "totalR\t" << "denseR\n";
  for (std::size_t x{0};x<IM_ENTRIES;++x){
    std::cout << IM[x].valid << "\t" << IM[x].pc <<"\t" << IM[x].totalR << "\t" << IM[x].denseR << std::endl;
  }
}
int count;


uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  /*
  if (cache_hit)
    debug::debug(CACHE::NAME,current_cycle,ip,addr,type);
*/

  for (int i{0};i<c1::IM_ENTRIES;++i){
    if (c1::IM[i].valid){
      c1::IM[i].age--;
      if (c1::IM[i].age == 0){
        if (c1::IM[i].totalR > c1::INSTRUCTION_DECISION_THRESHOLD){// evict IM 
          if (c1::IM[i].denseR >= 3){
            c1::dense_instruction.insert(c1::IM[i].pc);
          }
        }
        if (c1::IM[i].valid)
          c1::im_size--;
        c1::IM[i].valid = false;
        //std::cout << "IM Evict for oldage\n" ;
      }
    }

  }



  if (c1::dense_instruction.find(ip) != c1::dense_instruction.end()){
    //prefetch whole region
    //std::cout << "prefetch  " << ip << std::endl;
    for (uint64_t _addr = (( addr >> 6 ) << 6);_addr < ((( addr >> 6 )+16)<< 6);_addr+=64 )
      prefetch_line(_addr,true,metadata_in);

  }



  auto suspected = c1::search_rm(addr); 
  /*
  std::cout << "ip\t " << std::hex << ip 
            << " addr = "<< addr  <<" " << c1::byte2region(addr)
            <<" " << suspected << std::endl;
  */
  int instruction_loc = c1::find_IM_instruction(ip);

  if (instruction_loc == -1 ){
    instruction_loc = c1::insert_IM_instruction(ip);
  }
  



  //c1::print_IM();
  

  if (suspected == -1){
    c1::RM_entry evicted = c1::insert_rm_entry(addr);
    c1::set_cache_line(addr);
    if (evicted.age != -1){
      //std::cout << "have evicted"<<evicted.age << " " << evicted.region << "\n";
      int regions  = 0;
      for (std::size_t idx{0};idx<c1::REGION_SIZE;++idx)
        if (evicted.cache_line_bit_vector[idx])
          regions++;
      for (int w {0} ;w<c1::IM_ENTRIES;++w){
        if (evicted.pc_bit_vector[w]){
          c1::IM[w].totalR++;
          if (regions >= c1::DENSE_REGION_THRESHOLD)
            c1::IM[w].denseR++;
          if (c1::IM[w].totalR > c1::INSTRUCTION_DECISION_THRESHOLD){// evict IM 
            if (c1::IM[w].denseR >= 3){
              //std::cout << "MARK " << c1::IM[w].pc << std::endl;
              c1::dense_instruction.insert(c1::IM[w].pc);
            }
            if (c1::IM[w].valid)
              c1::im_size--;
            c1::IM[w].valid = false;
            
          }

        }
      }

    
    }
  }else{
    c1::update_RM_age(addr);
    c1::set_cache_line(addr);

  }
  
  if (instruction_loc != -1){
    auto rm_index = c1::search_rm(addr);
    if (rm_index != -1){
      c1::RM[rm_index].pc_bit_vector[instruction_loc] = true;
    }
  }
  if (false && count++>1000){
    c1::print_RM();
    count = 0;}
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}
