import sys
import argparse
import math

class CacheBlock:
    def __init__(self):
        self.valid = False
        self.tag = 0
        self.owner = -1
        self.last_used_cycle = 0

class Simulator:
    def __init__(self, num_set, num_way, superset_size, policy):
        self.num_set = num_set
        self.num_way = num_way
        self.superset_size = superset_size
        self.policy = policy 
        
        self.blocks = [[CacheBlock() for _ in range(num_way)] for _ in range(num_set)]
        self.num_supersets = (num_set + superset_size - 1) // superset_size
        self.header_trackers = [[0 for _ in range(num_way)] for _ in range(self.num_supersets)]
        
        self.total_shifts = 0
        self.cycle = 0
        self.hits = 0
        self.misses = 0

    def get_tag(self, address):
        index_bits = int(math.log2(self.num_set))
        return address >> (6 + index_bits)

    def access(self, cpu, set_idx, address):
        self.cycle += 1
        tag = self.get_tag(address)
        
        hit_way = -1
        for w in range(self.num_way):
            if self.blocks[set_idx][w].valid and self.blocks[set_idx][w].tag == tag:
                hit_way = w
                break
                
        if hit_way != -1:
            self.hits += 1
            way = hit_way
            self.blocks[set_idx][way].last_used_cycle = self.cycle
            self.blocks[set_idx][way].owner = cpu
        else:
            self.misses += 1
            way = self.find_victim(cpu, set_idx, tag)
            self.blocks[set_idx][way].valid = True
            self.blocks[set_idx][way].tag = tag
            self.blocks[set_idx][way].owner = cpu
            self.blocks[set_idx][way].last_used_cycle = self.cycle
            
        superset_idx = set_idx // self.superset_size
        local_set = set_idx % self.superset_size
        old_port = self.header_trackers[superset_idx][way]
        
        self.total_shifts += abs(local_set - old_port)
        self.header_trackers[superset_idx][way] = local_set

    def find_victim(self, cpu, set_idx, tag):
        local_set = set_idx % self.superset_size
        
        if self.policy.startswith('core_quota_'):
            quota = int(self.policy.split('_')[-1])
            
            # 1. Count owned ways and find Core LRU
            owned_in_set = set()
            core_lru_way = -1
            core_min_cycle = float('inf')
            
            for w in range(self.num_way):
                if self.blocks[set_idx][w].valid and self.blocks[set_idx][w].owner == cpu:
                    owned_in_set.add(w)
                    cycle = self.blocks[set_idx][w].last_used_cycle
                    if cycle < core_min_cycle:
                        core_min_cycle = cycle
                        core_lru_way = w
                        
            # 2. If quota reached, force self-eviction
            if len(owned_in_set) >= quota:
                return core_lru_way
                
            # 3. Try DBC Expansion from lower_set
            if local_set > 0:
                lower_set = set_idx - 1
                for w in range(self.num_way):
                    if self.blocks[lower_set][w].valid and self.blocks[lower_set][w].owner == cpu:
                        if w not in owned_in_set:
                            return w # Extend DBC
                            
            # 4. Fallback: Shift-minimizing expansion
            chosen_way = -1
            min_dist = float('inf')
            superset_idx = set_idx // self.superset_size
            
            for w in range(self.num_way):
                if w in owned_in_set:
                    continue # Do not expand into our own data
                    
                port_pos = self.header_trackers[superset_idx][w]
                dist = abs(local_set - port_pos)
                if dist < min_dist:
                    min_dist = dist
                    chosen_way = w
                elif dist == min_dist:
                    # tie-breaker: LRU
                    c1 = self.blocks[set_idx][w].last_used_cycle if self.blocks[set_idx][w].valid else -1
                    c2 = self.blocks[set_idx][chosen_way].last_used_cycle if chosen_way != -1 and self.blocks[set_idx][chosen_way].valid else -1
                    if chosen_way == -1 or c1 < c2:
                        chosen_way = w
                        
            if chosen_way != -1:
                return chosen_way
            return 0

        # Original ISCA logic
        if local_set > 0:
            lower_set = set_idx - 1
            for w in range(self.num_way):
                if self.blocks[lower_set][w].valid and self.blocks[lower_set][w].tag == tag:
                    return w
                        
        if local_set < self.superset_size - 1 and (set_idx + 1) < self.num_set:
            upper_set = set_idx + 1
            for w in range(self.num_way):
                if self.blocks[upper_set][w].valid and self.blocks[upper_set][w].tag == tag:
                    return w

        # Original ISCA Fallback
        chosen_way = 0
        min_dist = float('inf')
        superset_idx = set_idx // self.superset_size
        
        for w in range(self.num_way):
            port_pos = self.header_trackers[superset_idx][w]
            dist = abs(local_set - port_pos)
            if dist < min_dist:
                min_dist = dist
                chosen_way = w
            elif dist == min_dist:
                if self.blocks[set_idx][w].last_used_cycle < self.blocks[set_idx][chosen_way].last_used_cycle:
                    chosen_way = w
                    
        return chosen_way

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('log_file', type=str)
    args = parser.parse_args()
    
    sets, ways, superset = 2048, 16, 16
    sim_isca = Simulator(sets, ways, superset, 'isca')
    sim_q2 = Simulator(sets, ways, superset, 'core_quota_2')
    sim_q4 = Simulator(sets, ways, superset, 'core_quota_4')
    sim_q8 = Simulator(sets, ways, superset, 'core_quota_8')
    
    print(f"Reading log from {args.log_file}...")
    line_count = 0
    with open(args.log_file, 'r') as f:
        for line in f:
            parts = line.strip().replace(',', ' ').split()
            if len(parts) >= 3:
                try:
                    cpu = int(parts[1])
                    address = int(parts[2], 0)
                    set_idx = int(parts[3])
                    
                    sim_isca.access(cpu, set_idx, address)
                    sim_q2.access(cpu, set_idx, address)
                    sim_q4.access(cpu, set_idx, address)
                    sim_q8.access(cpu, set_idx, address)
                    
                    line_count += 1
                    if line_count % 2000000 == 0:
                        print(f"Processed {line_count} accesses...")
                except ValueError:
                    continue
                    
    print("\n" + "=" * 40)
    print("SIMULATION COMPLETE")
    print(f"Total Accesses Processed: {line_count}")
    print("=" * 40)
    print("1. ORIGINAL ISCA (Address-based)")
    print(f"   Hits:   {sim_isca.hits:,}")
    print(f"   Misses: {sim_isca.misses:,}")
    print(f"   SHIFTS: {sim_isca.total_shifts:,}")
    print("-" * 40)
    print("2. CORE QUOTA (Max 2 Ways)")
    print(f"   Hits:   {sim_q2.hits:,}")
    print(f"   Misses: {sim_q2.misses:,}")
    print(f"   SHIFTS: {sim_q2.total_shifts:,}")
    print("-" * 40)
    print("3. CORE QUOTA (Max 4 Ways)")
    print(f"   Hits:   {sim_q4.hits:,}")
    print(f"   Misses: {sim_q4.misses:,}")
    print(f"   SHIFTS: {sim_q4.total_shifts:,}")
    print("-" * 40)
    print("4. CORE QUOTA (Max 8 Ways)")
    print(f"   Hits:   {sim_q8.hits:,}")
    print(f"   Misses: {sim_q8.misses:,}")
    print(f"   SHIFTS: {sim_q8.total_shifts:,}")
    print("=" * 40)

if __name__ == '__main__':
    main()
