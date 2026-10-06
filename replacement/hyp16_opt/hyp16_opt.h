#ifndef REPLACEMENT_HYP16_OPT_H
#define REPLACEMENT_HYP16_OPT_H

#include <cstdint>
#include <deque>
#include <vector>
#include <fstream>
#include <string>

#include "cache.h"
#include "modules.h"

#define SUPERSET_SIZE 16

struct llc_access_record {
    uint32_t set;
    uint32_t way;
    uint32_t cpu;
    uint32_t type; // 0 = Hit/Access, 1 = Fill
    uint64_t clock;
    uint64_t full_addr;
};

class hyp16_opt : public champsim::modules::replacement {
        long NUM_WAY;
        std::vector<uint64_t> last_used_cycles;
        uint64_t cycle = 0;
        uint64_t total_shifts = 0;
        champsim::data::bits tag_start_bit{0};
        std::vector<uint16_t> header_trackers;
        long extra_cycle_w = 0;

        long num_lru_candidates;
        std::ofstream log_file;

        void log_access(uint32_t event_type, uint32_t cpu, long set, long way, champsim::address full_addr);

        struct HistEntry {
                uint32_t cpu;
                int local_set;
                uint32_t way;
        };
        std::vector<std::deque<HistEntry>> history_buffers;
        size_t max_hist_size = 32;

        bool is_streaming(uint32_t target_cpu, int superset_idx);

    public:
        explicit hyp16_opt(CACHE *cache);
        hyp16_opt(CACHE *cache, long sets, long ways);

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

        long extra_cycle();
};

#endif
