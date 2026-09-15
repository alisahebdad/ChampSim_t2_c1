#ifndef REPLACEMENT_HYPOTHESIS15_H
#define REPLACEMENT_HYPOTHESIS15_H

#include <cstdint>
#include <vector>
#include <deque>

#include "cache.h"
#include "modules.h"

#define SUPERSET_SIZE 16

class hypothesis15 : public champsim::modules::replacement {
        long NUM_WAY;
        std::vector<uint64_t> last_used_cycles;
        uint64_t cycle = 0;
        uint64_t total_shifts = 0;
        champsim::data::bits tag_start_bit{0};
        std::vector<uint16_t> header_trackers;

        long num_lru_candidates;
        
        struct HistEntry {
            uint32_t cpu;
            int local_set;
            uint32_t way;
        };
        std::vector<std::deque<HistEntry>> history_buffers;
        int max_hist_size = 32;

        bool is_streaming(uint32_t target_cpu, int superset_idx);

    public:
        explicit hypothesis15(CACHE *cache);
        hypothesis15(CACHE *cache, long sets, long ways);

        long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                         const champsim::cache_block *current_set,
                         champsim::address ip, champsim::address full_addr,
                         access_type type);
        void replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                                    champsim::address full_addr,
                                    champsim::address ip,
                                    champsim::address victim_addr,
                                    access_type type);
        void update_replacement_state(uint32_t triggering_cpu, long set,
                                      long way, champsim::address full_addr,
                                      champsim::address ip,
                                      champsim::address victim_addr,
                                      access_type type, uint8_t hit);
        void replacement_final_stats();
};

#endif
