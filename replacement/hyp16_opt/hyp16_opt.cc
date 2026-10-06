#include "hyp16_opt.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <unordered_set>

#include "util/bits.h"
#include <fmt/core.h>

constexpr long LRU_PERCENT = 50; // 50% of ways

hyp16_opt::hyp16_opt(CACHE *cache)
    : hyp16_opt(cache, cache->NUM_SET, cache->NUM_WAY) {
}

hyp16_opt::hyp16_opt(CACHE *cache, long sets, long ways)
    : replacement(cache), NUM_WAY(ways), last_used_cycles(sets * ways, 0),
      tag_start_bit(
          cache->OFFSET_BITS +
          champsim::data::bits{static_cast<uint64_t>(champsim::lg2(sets))}),
      header_trackers(((sets + SUPERSET_SIZE - 1) / SUPERSET_SIZE) * ways, 0),
      history_buffers((sets + SUPERSET_SIZE - 1) / SUPERSET_SIZE) {

    num_lru_candidates = std::max<long>(1, (NUM_WAY * LRU_PERCENT) / 100);
    num_lru_candidates = std::min<long>(num_lru_candidates, NUM_WAY);
}

bool hyp16_opt::is_streaming(uint32_t target_cpu, int superset_idx) {
    auto &hist = history_buffers[superset_idx];
    std::vector<int> core_sets;

    for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
        if (it->cpu == target_cpu) {
            core_sets.push_back(it->local_set);
            if (core_sets.size() >= 3)
                break;
        }
    }

    if (core_sets.size() >= 3) {
        int d1 = (core_sets[0] - core_sets[1] + SUPERSET_SIZE) % SUPERSET_SIZE;
        int d2 = (core_sets[1] - core_sets[2] + SUPERSET_SIZE) % SUPERSET_SIZE;
        if (d1 == d2 && (d1 == 1 || d1 == SUPERSET_SIZE - 1)) {
            return true;
        }
    }
    return false;
}

long hyp16_opt::find_victim(uint32_t triggering_cpu, uint64_t instr_id,
                            long set, const champsim::cache_block *current_set,
                            champsim::address ip, champsim::address full_addr,
                            access_type type) {
    uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);
    long superset_idx = set / SUPERSET_SIZE;
    champsim::address_slice<champsim::dynamic_extent> target_tag =
        full_addr.slice_upper(tag_start_bit);

    long candidates[128];
    for (long w = 0; w < NUM_WAY; ++w) {
        candidates[w] = w;
    }

    std::partial_sort(candidates, candidates + num_lru_candidates,
                      candidates + NUM_WAY, [&](long a, long b) {
                          return last_used_cycles[set * NUM_WAY + a] <
                                 last_used_cycles[set * NUM_WAY + b];
                      });

    long best_tier0_way = -1;
    int min_tier0_dist = std::numeric_limits<int>::max();

    auto check_tier0 = [&](long check_set) {
        for (long i = 0; i < num_lru_candidates; ++i) {
            long w = candidates[i];
            const champsim::cache_block &blk =
                intern_->block[check_set * NUM_WAY + w];
            if (blk.valid &&
                blk.address.slice_upper(tag_start_bit) == target_tag) {
                int port_pos = header_trackers[superset_idx * NUM_WAY + w];
                int dist = (local_set > port_pos) ? (local_set - port_pos)
                                                  : (port_pos - local_set);

                if (dist <= 2) {
                    if (dist < min_tier0_dist) {
                        min_tier0_dist = dist;
                        best_tier0_way = w;
                    }
                }
            }
        }
    };

    if (local_set > 0)
        check_tier0(set - 1);
    if (local_set < SUPERSET_SIZE - 1 && (set + 1) < intern_->NUM_SET)
        check_tier0(set + 1);

    auto &hist = history_buffers[superset_idx];
    std::vector<bool> streaming_cores(64, false);
    std::vector<bool> checked_cores(64, false);

    long best_tier1_way = -1;
    int min_tier1_dist = std::numeric_limits<int>::max();

    for (auto it = hist.begin(); it != hist.end(); ++it) {
        uint32_t c = it->cpu;
        if (c >= 64)
            continue;

        if (!checked_cores[c]) {
            streaming_cores[c] =
                is_streaming(c, static_cast<int>(superset_idx));
            checked_cores[c] = true;
        }

        if (it->local_set != local_set)
            continue;

        uint32_t w = it->way;
        int port_pos = header_trackers[superset_idx * NUM_WAY + w];
        int dist = (local_set > port_pos) ? (local_set - port_pos)
                                          : (port_pos - local_set);

        if (streaming_cores[c]) {
            if (dist < min_tier1_dist) {
                min_tier1_dist = dist;
                best_tier1_way = w;
            } else if (dist == min_tier1_dist && best_tier1_way != -1) {
                if (last_used_cycles[set * NUM_WAY + w] <
                    last_used_cycles[set * NUM_WAY + best_tier1_way]) {
                    best_tier1_way = w;
                }
            }
        }
    }

    std::unordered_set<uint32_t> active_cores;
    size_t active_cutoff = hist.size() / 2;
    size_t idx = 0;
    for (auto it = hist.begin(); it != hist.end(); ++it) {
        if (idx >= active_cutoff)
            active_cores.insert(it->cpu);
        idx++;
    }

    long best_tier2_way = -1;
    int min_tier2_dist = std::numeric_limits<int>::max();

    for (auto it = hist.begin(); it != hist.end(); ++it) {
        uint32_t c = it->cpu;
        if (active_cores.find(c) == active_cores.end() &&
            it->local_set == local_set) {
            uint32_t w = it->way;
            int port_pos = header_trackers[superset_idx * NUM_WAY + w];
            int dist = (local_set > port_pos) ? (local_set - port_pos)
                                              : (port_pos - local_set);
            if (dist < min_tier2_dist) {
                min_tier2_dist = dist;
                best_tier2_way = w;
            } else if (dist == min_tier2_dist && best_tier2_way != -1) {
                if (last_used_cycles[set * NUM_WAY + w] <
                    last_used_cycles[set * NUM_WAY + best_tier2_way]) {
                    best_tier2_way = w;
                }
            }
        }
    }

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

    if (best_tier1_way != -1 && min_tier1_dist <= min_dist) {
        return best_tier1_way;
    }

    if (best_tier2_way != -1 && min_tier2_dist <= min_dist) {
        return best_tier2_way;
    }

    if (best_tier0_way != -1 && min_tier0_dist <= min_dist) {
        return best_tier0_way;
    }

    return chosen_way;
}

void hyp16_opt::log_access(uint32_t event_type, uint32_t cpu, long set, long way, champsim::address full_addr) {
    if (intern_ && !intern_->warmup) {
        if (!log_file.is_open()) {
            const char* env_path = std::getenv("LLC_LOG_FILE");
            std::string filename = (env_path && env_path[0] != '\0') ? env_path : "llc_access_trace.bin";
            log_file.open(filename, std::ios::out | std::ios::binary | std::ios::app);
        }
        llc_access_record rec{
            static_cast<uint32_t>(set),
            static_cast<uint32_t>(way),
            cpu,
            event_type,
            static_cast<uint64_t>(intern_->current_time.time_since_epoch() / intern_->clock_period),
            full_addr.to<uint64_t>()
        };
        log_file.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
    }
}

void hyp16_opt::replacement_cache_fill(uint32_t triggering_cpu, long set,
                                       long way, champsim::address full_addr,
                                       champsim::address ip,
                                       champsim::address victim_addr,
                                       access_type type) {
    log_access(1, triggering_cpu, set, way, full_addr);

    long superset_idx = set / SUPERSET_SIZE;
    uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);

    history_buffers[superset_idx].push_back(
        HistEntry{triggering_cpu, local_set, static_cast<uint32_t>(way)});
    if (history_buffers[superset_idx].size() > max_hist_size) {
        history_buffers[superset_idx].pop_front();
    }

    uint16_t old_port = header_trackers[superset_idx * NUM_WAY + way];
    total_shifts += (local_set > old_port) ? (local_set - old_port)
                                           : (old_port - local_set);

    extra_cycle_w = (local_set > old_port) ? (local_set - old_port)
                                           : (old_port - local_set);
    header_trackers[superset_idx * NUM_WAY + way] = local_set;

    last_used_cycles[set * NUM_WAY + way] = cycle++;
}

void hyp16_opt::update_replacement_state(uint32_t triggering_cpu, long set,
                                         long way, champsim::address full_addr,
                                         champsim::address ip,
                                         champsim::address victim_addr,
                                         access_type type, uint8_t hit) {
    if (hit) {
        log_access(0, triggering_cpu, set, way, full_addr);

        long superset_idx = set / SUPERSET_SIZE;
        uint16_t local_set = static_cast<uint16_t>(set % SUPERSET_SIZE);

        history_buffers[superset_idx].push_back(
            HistEntry{triggering_cpu, local_set, static_cast<uint32_t>(way)});
        if (history_buffers[superset_idx].size() > max_hist_size) {
            history_buffers[superset_idx].pop_front();
        }

        uint16_t old_port = header_trackers[superset_idx * NUM_WAY + way];
        total_shifts += (local_set > old_port) ? (local_set - old_port)
                                               : (old_port - local_set);

        extra_cycle_w = (local_set > old_port) ? (local_set - old_port)
                                               : (old_port - local_set);

        header_trackers[superset_idx * NUM_WAY + way] = local_set;

        if (access_type{type} != access_type::WRITE) {
            last_used_cycles[set * NUM_WAY + way] = cycle++;
        }
    }
}

void hyp16_opt::replacement_final_stats() {
    if (log_file.is_open()) {
        log_file.flush();
        log_file.close();
    }
    fmt::print("HYP16_OPT Shifts: {}\n", total_shifts);
}

long hyp16_opt::extra_cycle() {
    return extra_cycle_w;
}
