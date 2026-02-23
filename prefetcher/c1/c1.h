#ifndef C1_H
#define C1_H
#include <set>

namespace c1 {

  constexpr int DENSE_REGION_THRESHOLD = 6;
  constexpr int INSTRUCTION_DECISION_THRESHOLD = 4;
  constexpr int IM_ENTRIES = 16;
  constexpr int RM_ENTRIES = 16;
  constexpr int REGION_SIZE = 16;
  constexpr int IM_MAX_AGE = 1000000;


  int search_rm(uint64_t addr);   //  return location of address region if exist either return -1 
  uint64_t byte2region(uint64_t addr);    //  convert byte address of memory to region tags 
 
  struct RM_entry {
    uint64_t region;
    int age;
    bool cache_line_bit_vector[REGION_SIZE];
    bool pc_bit_vector[IM_ENTRIES];
  };

  struct IM_entry {
    bool valid ;
    uint64_t pc ;
    int totalR;
    int denseR;
    int age ; 
  };

  static int im_size = 0;
  
  std::set<uint64_t> dense_instruction;
  int insert_IM_instruction(uint64_t ip);
  void evict_IM_instruction(uint64_t ip);
  int find_IM_instruction(uint64_t ip);
  bool full_IM();
  void print_IM();

 
  void print_RM();
  void reset_rm_entry(std::size_t);
  RM_entry insert_rm_entry(uint64_t addr);
  void set_cache_line(uint64_t addr); 
  void update_RM_age(uint64_t addr);

  static RM_entry RM[RM_ENTRIES];
  static IM_entry IM[IM_ENTRIES];



}



#endif
