#include "tapecache.h"
#include <fmt/core.h>

tapecache::tapecache(CACHE *cache)
    : tapecache(cache, cache->NUM_SET, cache->NUM_WAY) {
}

tapecache::tapecache(CACHE *cache, long sets, long ways)
    : replacement(cache), NUM_WAY(ways), last_used_cycles(sets * ways, 0),
      header_trackers(sets, 0) {
}

long tapecache::find_victim(uint32_t triggering_cpu, uint64_t instr_id,
                            long set, const champsim::cache_block *current_set,
                            champsim::address ip, champsim::address full_addr,
                            access_type type) {
    long chosen_way = 0;
    uint64_t min_cycle = last_used_cycles[set * NUM_WAY + 0];

    for (long w = 1; w < NUM_WAY; ++w) {
        if (last_used_cycles[set * NUM_WAY + w] < min_cycle) {
            min_cycle = last_used_cycles[set * NUM_WAY + w];
            chosen_way = w;
        }
    }
    return chosen_way;
}

void tapecache::replacement_cache_fill(uint32_t triggering_cpu, long set,
                                       long way, champsim::address full_addr,
                                       champsim::address ip,
                                       champsim::address victim_addr,
                                       access_type type) {
    uint16_t old_port = header_trackers[set];
    uint16_t target_way = static_cast<uint16_t>(way);

    uint16_t dist = (target_way > old_port) ? (target_way - old_port)
                                            : (old_port - target_way);
    total_shifts += dist;
    extra_cycle_w = dist;
    header_trackers[set] = target_way;

    last_used_cycles[set * NUM_WAY + way] = cycle++;
}

void tapecache::update_replacement_state(uint32_t triggering_cpu, long set,
                                         long way, champsim::address full_addr,
                                         champsim::address ip,
                                         champsim::address victim_addr,
                                         access_type type, uint8_t hit) {
    if (hit) {
        uint16_t old_port = header_trackers[set];
        uint16_t target_way = static_cast<uint16_t>(way);

        uint16_t dist = (target_way > old_port) ? (target_way - old_port)
                                                : (old_port - target_way);
        total_shifts += dist;
        extra_cycle_w = dist;
        header_trackers[set] = target_way;

        if (access_type{type} != access_type::WRITE) {
            last_used_cycles[set * NUM_WAY + way] = cycle++;
        }
    }
}

void tapecache::replacement_final_stats() {
    fmt::print("TAPECACHE Shifts: {}\n", total_shifts);
}

long tapecache::extra_cycle() {
    return extra_cycle_w;
}
