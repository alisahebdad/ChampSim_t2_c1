#include "logfiles.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <type_traits>


// ============================================================
// Constructor
// ============================================================

SimulationLogger::SimulationLogger(
    const std::string& directory,
    std::size_t buffer_size,
    std::chrono::seconds flush_interval)
    : directory_(directory),
      buffer_size_(buffer_size),
      flush_interval_(flush_interval),
      last_flush_(std::chrono::steady_clock::now())
{
    if (buffer_size_ == 0)
    {
        throw std::invalid_argument(
            "SimulationLogger: buffer size must be greater than zero");
    }

    // Create directory if it doesn't exist.
    std::filesystem::create_directories(directory_);

    // Reserve memory so vector does not repeatedly reallocate.
    buffer_.reserve(buffer_size_);

    // Open first file.
    open_new_file_locked();
}


// ============================================================
// Destructor
// ============================================================

SimulationLogger::~SimulationLogger()
{
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Write anything remaining in RAM.
        flush_locked();

        // Close current file.
        if (file_.is_open())
            file_.close();

        /*
         * If the current file contains data, compress it.
         *
         * This handles the final file which is normally
         * smaller than 200 MB.
         */
        if (current_file_size_ > 0)
        {
            const auto path = current_file_path();

            compress_file_locked(path);

            current_file_size_ = 0;
        }
    }
    catch (...)
    {
        // Never throw from a destructor.
    }
}


// ============================================================
// Current file path
// ============================================================

std::filesystem::path
SimulationLogger::current_file_path() const
{
    std::ostringstream filename;

    filename << "log_"
             << std::setfill('0')
             << std::setw(6)
             << file_index_
             << ".bin";

    return directory_ / filename.str();
}


// ============================================================
// Open new file
// ============================================================

void SimulationLogger::open_new_file_locked()
{
    const auto path = current_file_path();

    file_.open(
        path,
        std::ios::binary |
        std::ios::out |
        std::ios::trunc);

    if (!file_.is_open())
    {
        throw std::runtime_error(
            "SimulationLogger: could not open file: " +
            path.string());
    }

    current_file_size_ = 0;

    ++file_count_;
}


// ============================================================
// Rotate file
// ============================================================

void SimulationLogger::rotate_file_locked()
{
    // Make sure all buffered data has been written.
    flush_locked();

    if (file_.is_open())
        file_.close();

    const auto path = current_file_path();

    // Compress the completed file.
    compress_file_locked(path);

    // Move to next file.
    ++file_index_;

    // Open new file.
    open_new_file_locked();
}


// ============================================================
// Compress file
// ============================================================

void SimulationLogger::compress_file_locked(
    const std::filesystem::path& file_path)
{
    if (!std::filesystem::exists(file_path))
        return;

    const auto zip_path =
        file_path.string() + ".zip";

    /*
     * We need shell-safe paths because std::system()
     * invokes /bin/sh.
     *
     * This escapes:
     *   '
     *   \
     *   "
     *   $
     *   `
     *   etc.
     */
    auto shell_escape = [](const std::string& input)
    {
        std::string result = "'";

        for (char c : input)
        {
            if (c == '\'')
                result += "'\\''";
            else
                result += c;
        }

        result += "'";

        return result;
    };

    /*
     * -q  quiet
     * -j  don't store directory paths
     *
     * Example:
     *
     * zip -q -j 'log_000000.bin.zip' 'log_000000.bin'
     */
    const std::string command =
        "zip -q -j " +
        shell_escape(zip_path) +
        " " +
        shell_escape(file_path.string());

    const int result = std::system(command.c_str());

    if (result != 0)
    {
        throw std::runtime_error(
            "SimulationLogger: zip failed for file: " +
            file_path.string());
    }

    /*
     * Only delete the original file after zip
     * successfully returned.
     */
    std::error_code ec;

    std::filesystem::remove(file_path, ec);

    if (ec)
    {
        throw std::runtime_error(
            "SimulationLogger: could not delete file: " +
            file_path.string() +
            " : " +
            ec.message());
    }
}


// ============================================================
// Flush buffered data
// ============================================================

void SimulationLogger::flush_locked()
{
    if (buffer_.empty())
        return;

    const std::size_t bytes_to_write =
        buffer_.size();

    file_.write(
        reinterpret_cast<const char*>(buffer_.data()),
        static_cast<std::streamsize>(bytes_to_write));

    if (!file_)
    {
        throw std::runtime_error(
            "SimulationLogger: write failed");
    }

    current_file_size_ += bytes_to_write;

    total_bytes_ += bytes_to_write;

    buffer_.clear();

    last_flush_ =
        std::chrono::steady_clock::now();
}


// ============================================================
// Public flush
// ============================================================

void SimulationLogger::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);

    flush_locked();
}


// ============================================================
// Check whether buffer should be flushed
// ============================================================

void SimulationLogger::check_flush()
{
    const auto now =
        std::chrono::steady_clock::now();

    const bool buffer_full =
        buffer_.size() >= buffer_size_;

    const bool time_expired =
        now - last_flush_ >= flush_interval_;

    if (buffer_full || time_expired)
    {
        flush_locked();
    }
}


// ============================================================
// FIND_VICTIM
//
// Binary layout:
//
// uint8_t   event_type
// uint64_t  cycle
// uint32_t  triggering_cpu
// uint64_t  instr_id
// int64_t   set
// uint64_t  ip
// uint64_t  full_addr
// uint8_t   access_type
//
// Total = 46 bytes
// ============================================================

void SimulationLogger::log_find_victim(
    uint64_t cycle,
    uint32_t triggering_cpu,
    uint64_t instr_id,
    long set,
    champsim::address ip,
    champsim::address full_addr,
    access_type type)
{
    constexpr std::size_t EVENT_SIZE = 46;

    std::lock_guard<std::mutex> lock(mutex_);

    /*
     * Make sure this complete event fits in the
     * current 200 MB file.
     *
     * We rotate BEFORE writing the event.
     */
    if (current_file_size_ +
            buffer_.size() +
            EVENT_SIZE >
        MAX_FILE_SIZE)
    {
        flush_locked();

        rotate_file_locked();
    }

    const uint8_t event_type =
        static_cast<uint8_t>(EventType::FIND_VICTIM);

    const int64_t set_value =
        static_cast<int64_t>(set);

    const uint64_t ip_value =
        ip.to<uint64_t>();

    const uint64_t full_addr_value =
        full_addr.to<uint64_t>();

    const uint8_t access_type =
        static_cast<uint8_t>(type);

    append(event_type);
    append(cycle);
    append(triggering_cpu);
    append(instr_id);
    append(set_value);
    append(ip_value);
    append(full_addr_value);
    append(access_type);

    ++total_events_;

    check_flush();
}


// ============================================================
// REPLACEMENT_CACHE_FILL
//
// Binary layout:
//
// uint8_t   event_type
// uint64_t  cycle
// uint32_t  triggering_cpu
// int64_t   set
// int64_t   way
// uint64_t  full_addr
// uint64_t  ip
// uint64_t  victim_addr
// uint8_t   access_type
//
// Total = 54 bytes
// ============================================================

void SimulationLogger::log_replacement_cache_fill(
    uint64_t cycle,
    uint32_t triggering_cpu,
    long set,
    long way,
    champsim::address full_addr,
    champsim::address ip,
    champsim::address victim_addr,
    access_type type)
{
    constexpr std::size_t EVENT_SIZE = 54;

    std::lock_guard<std::mutex> lock(mutex_);

    if (current_file_size_ +
            buffer_.size() +
            EVENT_SIZE >
        MAX_FILE_SIZE)
    {
        flush_locked();

        rotate_file_locked();
    }

    const uint8_t event_type =
        static_cast<uint8_t>(
            EventType::REPLACEMENT_CACHE_FILL);

    const int64_t set_value =
        static_cast<int64_t>(set);

    const int64_t way_value =
        static_cast<int64_t>(way);

    const uint64_t full_addr_value =
        full_addr.to<uint64_t>();

    const uint64_t ip_value =
        ip.to<uint64_t>();

    const uint64_t victim_addr_value =
        victim_addr.to<uint64_t>();

    const uint8_t access_type =
        static_cast<uint8_t>(type);

    append(event_type);
    append(cycle);
    append(triggering_cpu);
    append(set_value);
    append(way_value);
    append(full_addr_value);
    append(ip_value);
    append(victim_addr_value);
    append(access_type);

    ++total_events_;

    check_flush();
}


// ============================================================
// UPDATE_REPLACEMENT_STATE
//
// Binary layout:
//
// uint8_t   event_type
// uint64_t  cycle
// uint32_t  triggering_cpu
// int64_t   set
// int64_t   way
// uint64_t  full_addr
// uint64_t  ip
// uint64_t  victim_addr
// uint8_t   access_type
// uint8_t   hit
//
// Total = 55 bytes
// ============================================================

void SimulationLogger::log_update_replacement_state(
    uint64_t cycle,
    uint32_t triggering_cpu,
    long set,
    long way,
    champsim::address full_addr,
    champsim::address ip,
    champsim::address victim_addr,
    access_type type,
    uint8_t hit)
{
    constexpr std::size_t EVENT_SIZE = 55;

    std::lock_guard<std::mutex> lock(mutex_);

    if (current_file_size_ +
            buffer_.size() +
            EVENT_SIZE >
        MAX_FILE_SIZE)
    {
        flush_locked();

        rotate_file_locked();
    }

    const uint8_t event_type =
        static_cast<uint8_t>(
            EventType::UPDATE_REPLACEMENT_STATE);

    const int64_t set_value =
        static_cast<int64_t>(set);

    const int64_t way_value =
        static_cast<int64_t>(way);

    const uint64_t full_addr_value =
        full_addr.to<uint64_t>();

    const uint64_t ip_value =
        ip.to<uint64_t>();

    const uint64_t victim_addr_value =
        victim_addr.to<uint64_t>();

    const uint8_t access_type =
        static_cast<uint8_t>(type);

    append(event_type);
    append(cycle);
    append(triggering_cpu);
    append(set_value);
    append(way_value);
    append(full_addr_value);
    append(ip_value);
    append(victim_addr_value);
    append(access_type);
    append(hit);

    ++total_events_;

    check_flush();
}


// ============================================================
// Statistics
// ============================================================

uint64_t SimulationLogger::get_total_events() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return total_events_;
}


uint64_t SimulationLogger::get_total_bytes() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return total_bytes_;
}


uint64_t SimulationLogger::get_file_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return file_count_;
}
