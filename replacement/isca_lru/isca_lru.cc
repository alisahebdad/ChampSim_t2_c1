#include "isca_lru.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "util/bits.h"
#include <fmt/core.h>

constexpr long LRU_PERCENT = 50; // 25% of ways

isca_lru::isca_lru(CACHE *cache) : isca_lru(cache, cache->NUM_SET, cache->NUM_WAY) {
}

isca_lru::isca_lru(CACHE *cache, long sets, long ways)
    : replacement(cache), NUM_WAY(ways), last_used_cycles(sets * ways, 0),
      tag_start_bit(
          cache->OFFSET_BITS +
          champsim::data::bits{static_cast<uint64_t>(champsim::lg2(sets))}),
      header_trackers(((sets + SUPERSET_SIZE - 1) / SUPERSET_SIZE) * ways, 0) {
          
    // Calculate K once at initialization
    num_lru_candidates = std::max<long>(1, (NUM_WAY * LRU_PERCENT) / 100);
    num_lru_candidates = std::min<long>(num_lru_candidates, NUM_WAY);
}

long isca_lru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                           const champsim::cache_block *current_set,
                           champsim::address ip, champsim::address full_addr,
                           access_type type) {
    uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);
    champsim::address_slice<champsim::dynamic_extent> target_tag =
        full_addr.slice_upper(tag_start_bit);

    // Identify the top K LRU candidates from the ORIGINAL set that caused the miss
    long candidates[128]; 
    for (long w = 0; w < NUM_WAY; ++w) {
        candidates[w] = w;
    }

    std::partial_sort(candidates, candidates + num_lru_candidates, candidates + NUM_WAY, [&](long a, long b) {
        return last_used_cycles[set * NUM_WAY + a] < last_used_cycles[set * NUM_WAY + b];
    });

    if (local_set > 0) {
        long lower_set = set - 1;
        for (long i = 0; i < num_lru_candidates; ++i) {
            long w = candidates[i];
            const champsim::cache_block &blk =
                intern_->block[lower_set * NUM_WAY + w];
            if (blk.valid &&
                blk.address.slice_upper(tag_start_bit) == target_tag) {
                return w;
            }
        }
    }

    if (local_set < SUPERSET_SIZE - 1 && (set + 1) < intern_->NUM_SET) {
        long upper_set = set + 1;
        for (long i = 0; i < num_lru_candidates; ++i) {
            long w = candidates[i];
            const champsim::cache_block &blk =
                intern_->block[upper_set * NUM_WAY + w];
            if (blk.valid &&
                blk.address.slice_upper(tag_start_bit) == target_tag) {
                return w;
            }
        }
    }

    long superset_idx = set / SUPERSET_SIZE;

    long chosen_way = candidates[0];
    int min_dist = std::numeric_limits<int>::max();

    for (long i = 0; i < num_lru_candidates; ++i) {
        long w = candidates[i];
        uint16_t port_pos = header_trackers[superset_idx * NUM_WAY + w];
        int dist = (local_set > port_pos) ? (local_set - port_pos)
                                          : (port_pos - local_set);
        if (dist < min_dist) {
            min_dist = dist;
            chosen_way = w;
        } else if (dist == min_dist) {
            if (last_used_cycles[set * NUM_WAY + w] <
                last_used_cycles[set * NUM_WAY + chosen_way]) {
                chosen_way = w;
            }
        }
    }

    return chosen_way;
}

void isca_lru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                                      champsim::address full_addr,
                                      champsim::address ip,
                                      champsim::address victim_addr,
                                      access_type type) {
    long superset_idx = set / SUPERSET_SIZE;
    uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);
    uint16_t old_port = header_trackers[superset_idx * NUM_WAY + way];

    uint16_t dist = (local_set > old_port) ? (local_set - old_port)
                                           : (old_port - local_set);
    total_shifts += dist;
    extra_cycle_w = dist;
    header_trackers[superset_idx * NUM_WAY + way] = local_set;

    last_used_cycles[set * NUM_WAY + way] = cycle++;
}

void isca_lru::update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                        champsim::address full_addr,
                                        champsim::address ip,
                                        champsim::address victim_addr,
                                        access_type type, uint8_t hit) {
    if (hit) {
        long superset_idx = set / SUPERSET_SIZE;
        uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);
        uint16_t old_port = header_trackers[superset_idx * NUM_WAY + way];

        uint16_t dist = (local_set > old_port) ? (local_set - old_port)
                                               : (old_port - local_set);
        total_shifts += dist;
        extra_cycle_w = dist;
        header_trackers[superset_idx * NUM_WAY + way] =
            local_set; // 0 to superset_size - 1

        if (access_type{type} != access_type::WRITE) {
            last_used_cycles[set * NUM_WAY + way] = cycle++;
        }
    }
}

void isca_lru::replacement_final_stats() {
    fmt::print("ISCA_LRU Shifts: {}\n", total_shifts);
}

long isca_lru::extra_cycle() {
    return extra_cycle_w;
}
