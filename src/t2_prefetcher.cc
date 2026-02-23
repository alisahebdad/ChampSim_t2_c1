#include "t2_prefetcher.h"
#include <cstddef>
#include <iostream>
#include <deque>  
#include "memory_trace.h"
namespace t2 {

  std::deque<uint64_t> function_stack;  
  std::vector<std::pair<uint64_t,int>> blacklist; // Pair < Ip, age> 
  std::vector <sit_entry> sit;
  std::set<uint64_t> non_stride;
  
  branch_target loop_branch_register;
  constexpr static int PREFETCH_BLACKLIST_SIZE = 20;
  constexpr static int MIN_SEG_SIZE = 3;

  bool log_write = false;

  bool is_in_blacklist(uint64_t ip){
    for (auto i = std::size_t{0} ;i<blacklist.size();++i){
      if (blacklist[i].first == ip)
        return true;
    }
    return false;

   }


  void update_blacklist(uint64_t ip){
    //print_blacklist();
    int loc = -1;
    int oldest = -1;
    int oldestIdx = -1;
    for (auto i = std::size_t{0};i<blacklist.size();++i){
      if (blacklist[i].second > oldest ){
        oldest = blacklist[i].second;
        oldestIdx = i; // find oldest Index position 
      }
      if (blacklist[i].first == ip)
        loc = i; // find ip in blacklist 
    }
    if (loc == -1){
      for (auto i = std::size_t{0} ;i<blacklist.size();++i)
        blacklist[i].second += 1;
      if (blacklist.size() < PREFETCH_BLACKLIST_SIZE ){
        blacklist.push_back(std::pair<uint64_t,int>(ip,0));
      }else{
        blacklist[oldestIdx].first = ip;
        blacklist[oldestIdx].second = 0;
      }
    }else{
      for (auto i = std::size_t{0};i<blacklist.size();++i){
        if(blacklist[i].second < blacklist[loc].second)
          blacklist[i].second++;
      }
      blacklist[loc].second = 0;
    }
  }

  inline void stack_predector(uint64_t ip, uint8_t branch_type, uint64_t branch_target){
    if (branch_type == branch_type::BRANCH_INDIRECT_CALL || branch_type == branch_type::BRANCH_DIRECT_CALL){
      function_stack.push_front(ip);
    }
    if (branch_type == branch_type::BRANCH_RETURN){
      //std::cout << "ret " << "ip =  " << std::hex << ip << " to = " << branch_target << std::endl;
      if (function_stack.size() > 1 &&  (branch_target - function_stack.front()) == 5 ){
        //std::cout << "pop " << std::hex << ip << "top " << function_stack.front() 
        //<< " brch = " << branch_target << " " << function_stack.size()  << std::endl;
        function_stack.pop_front();
      }
    }
    while (function_stack.size() > 50)
      function_stack.pop_back();
  }

  void branch_handler(uint64_t ip,[[maybe_unused]] uint8_t branch_type, uint64_t branch_target) {
    // first condition of condidate 
    stack_predector(ip,branch_type,branch_target);
    if (ip > branch_target) {       
      // second condtion of condidate
      if (!(is_in_blacklist(ip))){        
      //std::cout << "branch    " << std::hex << ip << " " <<(int) branch_type << std::endl;
        if (loop_branch_register.ip == ip){
          if (loop_branch_register.branch_target == branch_target){
            //if (loop_branch_register.valid == false)
            //  std::cout << "Valid = true" << " ip = " << std::hex << ip << " Brch = " << branch_target << std::endl;
            /*if (log_write == false){
              debug::debug("VALID",current_cycle,ip,1,0);
              log_write = true;
            }*/

            loop_branch_register.valid = true;
            loop_branch_register.count++;
            //std::cout << "loop ip " << ip << "loop target " << branch_target << " "  << loop_branch_register.count  << std::endl; 
          }else{
            //std::cout << "BACKLIST loop_branch_register DIFF Brch ip = " << std::hex << ip << "  Brch = " << branch_target << std::endl;
            update_blacklist(ip);
            //print_blacklist();
          }
        }else{
          if (
              !(  (loop_branch_register.ip > ip && loop_branch_register.branch_target < ip ) ||
                  (loop_branch_register.branch_target < branch_target && loop_branch_register.ip > branch_target) )){ 
            
            if (loop_branch_register.count < MIN_SEG_SIZE){
              //std::cout << "BLACKLIST loop_brach_register MIN_SEG_SIZE ip = " << std::hex 
              //          << loop_branch_register.ip << " Brch = " << loop_branch_register.branch_target << std::endl;  
              update_blacklist(loop_branch_register.ip);
            }
            //std::cout << "CONDIDATE " << std::hex << " ip = " << ip << " Brch = " 
            //          << branch_target << " CNT = " << std::dec << loop_branch_register.count << std::endl; 
            loop_branch_register.ip = ip ;
            loop_branch_register.branch_target = branch_target;
            loop_branch_register.valid = false;
            loop_branch_register.count = 1;
            /*if (log_write == true){
              debug::debug("VALID",current_cycle,ip,0,0);
              log_write = false;
            }*/
          }
        }
      }
    }    
  }



   

  void print_blacklist(){
    std::cout << "-----------------------------------------------\n";
    for (auto i = std::size_t{0} ;i<blacklist.size();++i)
      std::cout << std::dec << i << " IP " << std::hex << blacklist[i].first << " => " <<  blacklist[i].second << std::endl;
  }
 
  int search_sit(uint64_t mPC){
    for (auto i = std::size_t{0};i<sit.size();++i)
      if(sit[i].mPC == mPC)
        return i;
    return -1;   
  }


  void remove_sit(std::size_t i){
    if (i < sit.size())
      sit.erase(sit.begin() + i);
  }

  void print_sit_entry(sit_entry e){
    std::cout << "S" << e.state << "\tmPC:" << e.mPC<< "\tDelta: " << e.delta << " age " << e.age << std::endl;
  }

  void print_sit(){

    std::cout << "------------------------------------------------------\n";
    for (auto line:sit){
      print_sit_entry(line);
    }
  }

  uint64_t update_sit(uint64_t ip,uint64_t addr){
    //if (ip == 4336522)
    //std::cout << ip << " " << addr << " " << std::endl;
    //print_sit();
    //std::cout << sit.size() << std::endl;
    if (non_stride.find(ip) != non_stride.end() )
      return 0;;
    if (function_stack.size() > 0 )
      ip = function_stack.front() ^ ip ;
    auto i = search_sit(ip);
    if (i == -1){
      sit_entry new_entry {0,ip,addr,0,0,0};
      std::size_t oldesIdx=0;
      int oldest = -1;
      for (auto x = std::size_t{0};x<sit.size() ; ++x)
      {
        if (sit[x].age > oldest){
          oldest = sit[x].age;
          oldesIdx = x;
        }
        sit[x].age++;  
      }
      if (sit.size() > 32)
        sit.erase(sit.begin()+oldesIdx);
      
      sit.push_back(new_entry);
      //print_sit();
      //std::cout << "NEW :";
      //print_sit_entry(new_entry);
    }else{
      //if (sit[i].state > 2)
      //  std::cout << ip  << " " << addr << " " << sit[i].state << std::endl;
      for (auto x = std::size_t{0};x<sit.size();++x)
        if (sit[x].age < sit[i].age)
          sit[x].age++;
      sit[i].age = 0;


      int64_t new_delta = addr - sit[i].lastAddr;
      //std::cout << "+" << ip <<  ":" << addr << std::endl;
      //print_sit_entry(sit[i]); 
      if (new_delta == 0 ) {// There is not any delta 
        non_stride.insert(ip);
      }
      else{
        if (sit[i].delta == new_delta){ // predict corret
          if (sit[i].state < 0) sit[i].state = 1;
          else sit[i].state = std::min (sit[i].state+1,4);
          
        }else{// wrong predict 
          if (sit[i].state < 0 ) sit[i].state = std::max(sit[i].state - 1,-4);
          else sit[i].state = -1;
        }

      }

      sit[i].delta = new_delta;
      sit[i].lastAddr = addr;
      sit[i].lastPref = addr+new_delta;
      //print_sit();
      //print_sit_entry(sit[i]);
      //print_sit();
      if (sit[i].state == 4){
        return sit[i].lastPref;
      }

      if (sit[i].state == -4){
        remove_sit(i);        
        //print_sit();
      }
    }
    return 0;
  }


}

