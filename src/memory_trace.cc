#include "memory_trace.h"
#include <fstream>


std::fstream debug::fout;
bool debug::initiate = false;
bool debug::inLoop;
long long debug::memory_access_count;
long long debug::total_access;
std::fstream debug::access_per_second;

void debug::debug(std::string name,int clk,uint64_t ip,uint64_t addr,uint8_t type) {
  
  if (debug::initiate == false)
    debug::initializer();
  debug::fout << name << "," << clk << "," << ip << "," << addr << "," << (int)type <<  std::endl;

}  



void debug::initializer(){
  debug::initiate = true;
  debug::fout.open("main_memory_access.txt", std::ios::out);
  debug::access_per_second.open("access_per_second.txt",std::ios::out);
}
