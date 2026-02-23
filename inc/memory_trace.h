#ifndef MEMORY_TRACE 
#define MEMORY_TRACE
#pragma once 
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>

namespace debug {
  extern std::fstream fout ; 
  extern bool initiate;
  extern bool inLoop;
  extern long long memory_access_count;
  extern long long total_access;
  extern std::fstream access_per_second;


  void initializer();
  void debug(std::string name,int clk,uint64_t ip,uint64_t addr,uint8_t type);  

}


#endif 
