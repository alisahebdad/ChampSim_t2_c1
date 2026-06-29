#ifndef CSV_MANAGER_H
#define CSV_MANAGER_H

#include <fstream>
#include <string>
#include <vector>

class CsvWriter {
public:
    // Construct with the target file path. Optionally overwrite (truncate) an existing file.
    explicit CsvWriter(const std::string& filePath, bool overwrite = true)
        : filePath_(filePath) {
        // Opening in truncate mode clears any existing content; append mode keeps it.
        std::ofstream file(filePath_, overwrite ? std::ios::trunc : std::ios::app);
    }

    // 1) Add a header row from a vector of strings.
    bool addHeader(const std::vector<std::string>& headers) {
        std::ofstream file(filePath_, std::ios::app);
        if (!file.is_open()) return false;

        for (size_t i = 0; i < headers.size(); ++i) {
            if (i > 0) file << ',';
            file << escape(headers[i]);
        }
        file << '\n';
        return file.good();
    }

    // 2) Append a data row from a vector of long long.
    bool appendRow(const std::vector<long long>& values) {
        std::ofstream file(filePath_, std::ios::app);
        if (!file.is_open()) return false;

        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) file << ',';
            file << values[i];
        }
        file << '\n';
        return file.good();
    }

    // 3) Make sure data is saved/flushed to disk.
    //    Each call above opens, writes, and closes (so it's flushed already),
    //    but this gives an explicit "save" you can call and check.
    bool save() {
        std::ofstream file(filePath_, std::ios::app);
        if (!file.is_open()) return false;
        file.flush();
        return file.good();
    }

private:
    std::string filePath_;

    // Quote fields that contain commas, quotes, or newlines (basic CSV escaping).
    static std::string escape(const std::string& field) {
        if (field.find_first_of(",\"\n") == std::string::npos)
            return field;
        std::string out = "\"";
        for (char c : field) {
            if (c == '"') out += '"';  // double the quote
            out += c;
        }
        out += '"';
        return out;
    }
};

#endif 
