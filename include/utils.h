#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

namespace utils {
    std::string read_file(const std::string& path);

    void write_file(const std::string& path, const std::string& data);

    std::string sha1(const std::string& data);
    //better name be hash than sha1

    std:: string hex_to_bytes(const std::string& hex);
    std::string compress(const std::string& data);

    std::string decompress(const std::string& data);

}

#endif
