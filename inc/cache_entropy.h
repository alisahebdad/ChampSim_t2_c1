#ifndef CACHE_ENTROPY_H
#define CACHE_ENTROPY_H

// cache_entropy.hpp
//
// Computes the normalized conditional entropy (Hnorm) of cache way-access
// patterns, as defined by:
//
//   H_s      = H(W_{t+1}^(s) | W_t^(s))                     (per-set conditional entropy)
//   H_cache  = sum_s (N_s / sum_k N_k) * H_s                (access-weighted average)
//   H_norm   = H_cache / log2(A)                            (normalized to [0,1])
//
// Usage:
//   CacheEntropyCalculator calc(numSets, associativity);
//   for each trace access (set, way):
//       calc.recordAccess(set, way);
//   double hnorm = calc.computeHnorm();

#include <cstdint>
#include <cmath>
#include <vector>
#include <stdexcept>

class CacheEntropyCalculator {
public:
    CacheEntropyCalculator(std::size_t numSets, std::size_t associativity)
        : S_(numSets),
          A_(associativity),
          lastWay_(numSets, -1),
          // transitionCounts_[s][i][j] = N_s(i -> j)
          transitionCounts_(numSets,
              std::vector<std::vector<std::uint64_t>>(
                  associativity, std::vector<std::uint64_t>(associativity, 0)))
    {
        if (S_ == 0 || A_ == 0) {
            throw std::invalid_argument("numSets and associativity must be > 0");
        }
    }

    // Record that way `way` was accessed in cache set `set` at the current
    // (next) time step. Internally tracks the previous way accessed in that
    // set so it can increment N_s(prev -> way).
    void recordAccess(std::size_t set, int way) {
        if (set >= S_) {
            throw std::out_of_range("set index out of range");
        }
        if (way < 0 || static_cast<std::size_t>(way) >= A_) {
            throw std::out_of_range("way index out of range");
        }

        int prev = lastWay_[set];
        if (prev >= 0) {
            transitionCounts_[set][static_cast<std::size_t>(prev)][static_cast<std::size_t>(way)]++;
        }
        lastWay_[set] = way;
    }

    // Reset all accumulated statistics (keeps S and A).
    void reset() {
        for (auto& mat : transitionCounts_) {
            for (auto& row : mat) {
                std::fill(row.begin(), row.end(), 0);
            }
        }
        std::fill(lastWay_.begin(), lastWay_.end(), -1);
    }

    // H_s = -sum_i P_s(i) * sum_j P_s(j|i) * log2(P_s(j|i))
    // Computed directly from counts:
    // H_s = -sum_i (N_s(i)/N_s) * sum_j (N_s(i->j)/N_s(i)) * log2(N_s(i->j)/N_s(i))
    double computeSetEntropy(std::size_t set) const {
        if (set >= S_) {
            throw std::out_of_range("set index out of range");
        }

        const auto& mat = transitionCounts_[set];

        // N_s(i) = sum_j N_s(i->j), N_s = sum_i N_s(i)
        std::vector<std::uint64_t> Ni(A_, 0);
        std::uint64_t Ns = 0;
        for (std::size_t i = 0; i < A_; ++i) {
            std::uint64_t rowSum = 0;
            for (std::size_t j = 0; j < A_; ++j) {
                rowSum += mat[i][j];
            }
            Ni[i] = rowSum;
            Ns += rowSum;
        }

        if (Ns == 0) {
            return 0.0; // no transitions recorded for this set
        }

        double Hs = 0.0;
        for (std::size_t i = 0; i < A_; ++i) {
            if (Ni[i] == 0) continue;
            double Pi = static_cast<double>(Ni[i]) / static_cast<double>(Ns);

            double innerSum = 0.0; // sum_j P(j|i) * log2(P(j|i))
            for (std::size_t j = 0; j < A_; ++j) {
                std::uint64_t nij = mat[i][j];
                if (nij == 0) continue;
                double pij = static_cast<double>(nij) / static_cast<double>(Ni[i]);
                innerSum += pij * std::log2(pij);
            }
            Hs += Pi * (-innerSum);
        }
        return Hs;
    }

    // Total transitions recorded for a set: N_s = sum_i sum_j N_s(i->j)
    std::uint64_t getSetTransitionCount(std::size_t set) const {
        if (set >= S_) {
            throw std::out_of_range("set index out of range");
        }
        std::uint64_t Ns = 0;
        for (const auto& row : transitionCounts_[set]) {
            for (auto v : row) Ns += v;
        }
        return Ns;
    }

    // Returns H_s for every set, indexed by set id (0..S-1).
    std::vector<double> computeAllSetEntropies() const {
        std::vector<double> Hs(S_, 0.0);
        for (std::size_t s = 0; s < S_; ++s) {
            Hs[s] = computeSetEntropy(s);
        }
        return Hs;
    }

    // H_cache = sum_s (N_s / sum_k N_k) * H_s
    double computeHcache() const {
        std::vector<std::uint64_t> Ns(S_, 0);
        std::uint64_t totalN = 0;
        for (std::size_t s = 0; s < S_; ++s) {
            Ns[s] = getSetTransitionCount(s);
            totalN += Ns[s];
        }

        if (totalN == 0) {
            return 0.0; // no data recorded at all
        }

        double Hcache = 0.0;
        for (std::size_t s = 0; s < S_; ++s) {
            if (Ns[s] == 0) continue;
            double weight = static_cast<double>(Ns[s]) / static_cast<double>(totalN);
            Hcache += weight * computeSetEntropy(s);
        }
        return Hcache;
    }

    // H_norm = H_cache / log2(A)
    double computeHnorm() const {
        if (A_ <= 1) {
            // log2(1) = 0 -> undefined / trivially zero entropy possibilities
            return 0.0;
        }
        double Hcache = computeHcache();
        return Hcache / std::log2(static_cast<double>(A_));
    }

    std::size_t numSets() const { return S_; }
    std::size_t associativity() const { return A_; }

private:
    std::size_t S_; // number of cache sets
    std::size_t A_; // cache associativity

    std::vector<int> lastWay_; // last accessed way per set (-1 = none yet)

    // transitionCounts_[s][i][j] = N_s(i -> j)
    std::vector<std::vector<std::vector<std::uint64_t>>> transitionCounts_;
};


#endif
