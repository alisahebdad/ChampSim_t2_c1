/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LOGFILES_H
#define LOGFILES_H

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "cache.h"
#include "champsim.h"

class SimulationLogger
{
public:
    // RAM buffer used before writing to disk.
    static constexpr std::size_t DEFAULT_BUFFER_SIZE =
        16ULL * 1024 * 1024; // 16 MB

    // Maximum uncompressed size of each log file.
    static constexpr std::size_t MAX_FILE_SIZE =
        200ULL * 1024 * 1024; // 200 MB

    static constexpr std::chrono::seconds DEFAULT_FLUSH_INTERVAL{
        10
    };

    explicit SimulationLogger(
        const std::string& directory,
        std::size_t buffer_size = DEFAULT_BUFFER_SIZE,
        std::chrono::seconds flush_interval = DEFAULT_FLUSH_INTERVAL);

    ~SimulationLogger();

    SimulationLogger(const SimulationLogger&) = delete;
    SimulationLogger& operator=(const SimulationLogger&) = delete;

    // ------------------------------------------------------------
    // Logging functions
    // ------------------------------------------------------------

    void log_find_victim(
        uint64_t cycle,
        uint32_t triggering_cpu,
        uint64_t instr_id,
        long set,
        champsim::address ip,
        champsim::address full_addr,
        access_type type);

    void log_replacement_cache_fill(
        uint64_t cycle,
        uint32_t triggering_cpu,
        long set,
        long way,
        champsim::address full_addr,
        champsim::address ip,
        champsim::address victim_addr,
        access_type type);

    void log_update_replacement_state(
        uint64_t cycle,
        uint32_t triggering_cpu,
        long set,
        long way,
        champsim::address full_addr,
        champsim::address ip,
        champsim::address victim_addr,
        access_type type,
        uint8_t hit);

    // Force buffered data to disk.
    void flush();

    // ------------------------------------------------------------
    // Statistics
    // ------------------------------------------------------------

    uint64_t get_total_events() const;
    uint64_t get_total_bytes() const;
    uint64_t get_file_count() const;

private:
    enum class EventType : uint8_t
    {
        FIND_VICTIM = 0,
        REPLACEMENT_CACHE_FILL = 1,
        UPDATE_REPLACEMENT_STATE = 2
    };

    // ------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------

    std::filesystem::path directory_;

    std::size_t buffer_size_;
    std::chrono::seconds flush_interval_;

    // ------------------------------------------------------------
    // Current file
    // ------------------------------------------------------------

    std::ofstream file_;

    // Number of bytes already written to current .bin file.
    std::size_t current_file_size_ = 0;

    // log_000000.bin, log_000001.bin, ...
    uint64_t file_index_ = 0;

    // ------------------------------------------------------------
    // RAM buffer
    // ------------------------------------------------------------

    std::vector<uint8_t> buffer_;

    std::chrono::steady_clock::time_point last_flush_;

    // ------------------------------------------------------------
    // Statistics
    // ------------------------------------------------------------

    uint64_t total_events_ = 0;
    uint64_t total_bytes_ = 0;
    uint64_t file_count_ = 0;

    // ------------------------------------------------------------
    // Synchronization
    // ------------------------------------------------------------

    mutable std::mutex mutex_;

    // ------------------------------------------------------------
    // Internal functions
    // ------------------------------------------------------------

    void check_flush();

    void flush_locked();

    void open_new_file_locked();

    void rotate_file_locked();

    void compress_file_locked(
        const std::filesystem::path& file_path);

    std::filesystem::path current_file_path() const;

    // Add raw bytes to the buffer.
    template <typename T>
    void append(const T& value)
    {
        static_assert(
            std::is_trivially_copyable_v<T>,
            "T must be trivially copyable");

        const auto* ptr =
            reinterpret_cast<const uint8_t*>(&value);

        buffer_.insert(
            buffer_.end(),
            ptr,
            ptr + sizeof(T));
    }
};




  
#endif
