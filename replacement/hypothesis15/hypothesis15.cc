#include "hypothesis15.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>

#include "util/bits.h"
#include <fmt/core.h>

constexpr long LRU_PERCENT = 50; // 50% of ways

hypothesis15::hypothesis15(CACHE *cache) : hypothesis15(cache, cache->NUM_SET, cache->NUM_WAY) {
}

hypothesis15::hypothesis15(CACHE *cache, long sets, long ways)
    : replacement(cache), NUM_WAY(ways), last_used_cycles(sets * ways, 0),
      tag_start_bit(
          cache->OFFSET_BITS +
          champsim::data::bits{static_cast<uint64_t>(champsim::lg2(sets))}),
      header_trackers(((sets + SUPERSET_SIZE - 1) / SUPERSET_SIZE) * ways, 0),
      history_buffers((sets + SUPERSET_SIZE - 1) / SUPERSET_SIZE) {
          
    num_lru_candidates = std::max<long>(1, (NUM_WAY * LRU_PERCENT) / 100);
    num_lru_candidates = std::min<long>(num_lru_candidates, NUM_WAY);
}

bool hypothesis15::is_streaming(uint32_t target_cpu, int superset_idx) {
    auto& hist = history_buffers[superset_idx];
    std::vector<int> core_sets;
    
    for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
        if (it->cpu == target_cpu) {
            core_sets.push_back(it->local_set);
            if (core_sets.size() >= 3) break;
        }
    }
    
    if (core_sets.size() >= 3) {
        int d1 = (core_sets[0] - core_sets[1] + SUPERSET_SIZE) % SUPERSET_SIZE;
        int d2 = (core_sets[1] - core_sets[2] + SUPERSET_SIZE) % SUPERSET_SIZE;
        if (d1 == d2 && d1 != 0) {
            return true;
        }
    }
    return false;
}

long hypothesis15::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                               const champsim::cache_block *current_set,
                               champsim::address ip, champsim::address full_addr,
                               access_type type) {
    uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);
    long superset_idx = set / SUPERSET_SIZE;
    champsim::address_slice<champsim::dynamic_extent> target_tag =
        full_addr.slice_upper(tag_start_bit);

    // 1. STANDARD ISCA CANDIDATES
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
            const champsim::cache_block &blk = intern_->block[lower_set * NUM_WAY + w];
            if (blk.valid && blk.address.slice_upper(tag_start_bit) == target_tag) {
                return w;
            }
        }
    }

    if (local_set < SUPERSET_SIZE - 1 && (set + 1) < intern_->NUM_SET) {
        long upper_set = set + 1;
        for (long i = 0; i < num_lru_candidates; ++i) {
            long w = candidates[i];
            const champsim::cache_block &blk = intern_->block[upper_set * NUM_WAY + w];
            if (blk.valid && blk.address.slice_upper(tag_start_bit) == target_tag) {
                return w;
            }
        }
    }

    // 2. TIERED BURST SCAVENGER (Tier 1: Streaming, Tier 2: Inactive)
    auto& hist = history_buffers[superset_idx];
    
    std::set<uint32_t> active_cores;
    int idx = 0;
    int active_cutoff = hist.size() / 2;
    for (auto it = hist.begin(); it != hist.end(); ++it) {
        if (idx >= active_cutoff) {
            active_cores.insert(it->cpu);
        }
        idx++;
    }

    std::vector<bool> streaming_cores(64, false);
    std::vector<bool> inactive_cores(64, false);
    std::vector<bool> checked_cores(64, false);
    
    long best_tier1_way = -1;
    int min_tier1_dist = std::numeric_limits<int>::max();
    
    long best_tier2_way = -1;
    int min_tier2_dist = std::numeric_limits<int>::max();
    
    for (auto it = hist.begin(); it != hist.end(); ++it) {
        uint32_t c = it->cpu;
        if (!checked_cores[c]) {
            streaming_cores[c] = is_streaming(c, superset_idx);
            if (active_cores.find(c) == active_cores.end()) {
                inactive_cores[c] = true;
            }
            checked_cores[c] = true;
        }
        
        uint32_t w = it->way;
        int port_pos = header_trackers[superset_idx * NUM_WAY + w];
        int dist = (local_set > port_pos) ? (local_set - port_pos) : (port_pos - local_set);
        
        if (streaming_cores[c]) {
            if (dist < min_tier1_dist) {
                min_tier1_dist = dist;
                best_tier1_way = w;
            } else if (dist == min_tier1_dist && best_tier1_way != -1) {
                if (last_used_cycles[set * NUM_WAY + w] < last_used_cycles[set * NUM_WAY + best_tier1_way]) {
                    best_tier1_way = w;
                }
            }
        }
        
        if (inactive_cores[c]) {
            if (dist < min_tier2_dist) {
                min_tier2_dist = dist;
                best_tier2_way = w;
            } else if (dist == min_tier2_dist && best_tier2_way != -1) {
                if (last_used_cycles[set * NUM_WAY + w] < last_used_cycles[set * NUM_WAY + best_tier2_way]) {
                    best_tier2_way = w;
                }
            }
        }
    }

    // 3. BEST STANDARD WAY
    long chosen_way = candidates[0];
    int min_dist = std::numeric_limits<int>::max();

    for (long i = 0; i < num_lru_candidates; ++i) {
        long w = candidates[i];
        uint16_t port_pos = header_trackers[superset_idx * NUM_WAY + w];
        int dist = (local_set > port_pos) ? (local_set - port_pos) : (port_pos - local_set);
        
        if (dist < min_dist) {
            min_dist = dist;
            chosen_way = w;
        } else if (dist == min_dist) {
            if (last_used_cycles[set * NUM_WAY + w] < last_used_cycles[set * NUM_WAY + chosen_way]) {
                chosen_way = w;
            }
        }
    }

    // CASCADE SELECTION:
    if (best_tier1_way != -1 && min_tier1_dist <= min_dist) {
        return best_tier1_way; // Tier 1: Safest (Dead stream)
    }
    
    if (best_tier2_way != -1 && min_tier2_dist < min_dist) {
        // Tier 2: Stalled cores. We only use them if they offer a strictly better shift
        // than the standard LRU pool, because taking from Tier 2 bypasses LRU.
        return best_tier2_way; 
    }

    return chosen_way; // Tier 3: Standard
}

void hypothesis15::replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                                          champsim::address full_addr,
                                          champsim::address ip,
                                          champsim::address victim_addr,
                                          access_type type) {
    long superset_idx = set / SUPERSET_SIZE;
    uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);
    
    // Update history
    history_buffers[superset_idx].push_back(HistEntry{triggering_cpu, local_set, static_cast<uint32_t>(way)});
    if (history_buffers[superset_idx].size() > max_hist_size) {
        history_buffers[superset_idx].pop_front();
    }
    
    uint16_t old_port = header_trackers[superset_idx * NUM_WAY + way];
    total_shifts += (local_set > old_port) ? (local_set - old_port) : (old_port - local_set);
    header_trackers[superset_idx * NUM_WAY + way] = local_set;

    last_used_cycles[set * NUM_WAY + way] = cycle++;
}

void hypothesis15::update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                            champsim::address full_addr,
                                            champsim::address ip,
                                            champsim::address victim_addr,
                                            access_type type, uint8_t hit) {
    if (hit) {
        long superset_idx = set / SUPERSET_SIZE;
        uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);
        
        // Update history
        history_buffers[superset_idx].push_back(HistEntry{triggering_cpu, local_set, static_cast<uint32_t>(way)});
        if (history_buffers[superset_idx].size() > max_hist_size) {
            history_buffers[superset_idx].pop_front();
        }

        uint16_t old_port = header_trackers[superset_idx * NUM_WAY + way];
        total_shifts += (local_set > old_port) ? (local_set - old_port) : (old_port - local_set);
        header_trackers[superset_idx * NUM_WAY + way] = local_set;

        if (access_type{type} != access_type::WRITE) {
            last_used_cycles[set * NUM_WAY + way] = cycle++;
        }
    }
}

void hypothesis15::replacement_final_stats() {
    fmt::print("HYPOTHESIS15 Shifts: {}\n", total_shifts);
}
