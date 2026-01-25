#include "utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <zlib.h>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace utils {
    std::string read_file(const std::string& path){
        std::ifstream file(path, std::ios::binary);
        if(!file){
            throw std::runtime_error("Could not open file: " + path);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
        // the rdbuf() function for an std::ifstream returns a pointer to the 
        // underlying stream buffer object associated with the input file stream.
        // The stream buffer, specifically a std::filebuf for ifstream, is the low-level
        // responsible for the actual I/O oprtns. [it uses arrays in multiple rounds, doesnt read at once]
        // rdbuf() provides direct access to this buffer, allowing for efficient reading/writing.
        // "<<" -- operator is overloaded for stream insertion."
	    // The read_file returns string for a binary file because std::string is continguous byte
	    // container. It doesnt use the null terminator as end marker, but rather has the size info
	    // explicitly stored. So it can hold any binary data including null bytes.
	    // Thus, string is the safe choice for bianry data.
    void write_file(const std:: string& path, const std::string& data){
        fs::path file_path(path);
        if(file_path.has_parent_path()){
            fs::create_directories(file_path.parent_path());
        }
        std::ofstream file(path, std::ios::binary);
        if(!file){
            throw std::runtime_error("Could not write to file: " + path);
        }
        file.write(data.data(), data.size());
    }

    std::string sha1(const std::string& data) {
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

        std::stringstream ss;

        for(int i = 0; i < SHA_DIGEST_LENGTH; i++){
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();

    }
     
	    // reinterpret_cast is used to convert pointer types without changing the bit pattern.
        // used in conversion of pointer types, even if they are unreleted.
        // Unsafed, but here we know that c_str() gives us a valid pointer to bytes.
       // std::hex switches the stream to hexadecimal output.
        // std::setw(2) ensures each byte is printed using exactly two hex digits.
        // std::setfill('0') pads with a leading 0 when needed.
        // (int)hash[i] converts the byte to an integer so it prints as a number instead of a character.
        // The result is appended to the stringstream ss

    std::string hex_to_bytes(const std::string& hex){
        std::string bytes;
        for(unsigned int i = 0; i < hex.length(); i+=2){
            std::string byteString = hex.substr(i,2);
            char byte = (char) strtol(byteString.c_str(), nullptr, 16);
            bytes.push_back(byte);
        }
        return bytes;
        // strtol (string to long)converts a string representation of a number
            // into a long int value, can handle whitespace, signs, 
            // different bases (2-36), and providing an endptr to find where 
            // the conversion stopped.
    }

    std::string compress(const std::string& data){
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;

        if(deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
            throw std::runtime_error("Failed to initialize zlib compression.");
        }

        zs.next_in = (Bytef*)data.data();
        zs.avail_in = data.size();

        int ret;
        char outbuffer[32768];
        std::string outstring;

        do{
            zs.next_out = (Bytef*)outbuffer;
            zs.avail_out = sizeof(outbuffer);

            ret = deflate(&zs, Z_FINISH);

            if(outstring.size() < zs.total_out){
                outstring.append(outbuffer, zs.total_out - outstring.size());
            }
        } while(ret == Z_OK);

        deflateEnd(&zs);

        if (ret != Z_STREAM_END) {
            throw std::runtime_error("Exception during zlib compression. " + std::to_string(ret));
        }
        return outstring;
    }

    std::string decompress(const std::string& data){
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.next_in = (Bytef*)data.data();
        zs.avail_in = data.size();

        if(inflateInit(&zs) != Z_OK) {
            throw std::runtime_error("Failed to initialize zlib decompression. ");
        }

        int ret;
        char outbuffer[32768];
        std::string outstring;
        do{
            zs.next_out = (Bytef*)outbuffer;
            zs.avail_out = sizeof(outbuffer);

            ret = inflate(&zs, 0);

            if(outstring.size() < zs.total_out){
                outstring.append(outbuffer, zs.total_out - outstring.size());
            }
        }
        while (ret == Z_OK);
        inflateEnd(&zs);
        return outstring;
    }

    void print_storage_stats() {
        long total_compressed = 0;
        long total_raw = 0;
        int object_count = 0;

        if (!fs::exists(".mvc/objects")) {
             std::cout << "No repository found.\n";
             return;
        }

        for (const auto& dir : fs::recursive_directory_iterator(".mvc/objects")) {
            if (!fs::is_regular_file(dir)) continue;
            
            // Get compressed size --> (disk usage)
            std::string compressed_content = read_file(dir.path().string());
            total_compressed += compressed_content.size();
            
            // Get raw size 
            try {
                std::string raw = decompress(compressed_content);
                total_raw += raw.size();
                object_count++;
            } catch (...) {
                // Ignore corrupt/empty files
            }
        }

        if (total_raw == 0) total_raw = 1; // Avoid divide by zero

        double savings = 100.0 * (1.0 - (double)total_compressed / total_raw);
        
        std::cout << ">>> METRICS GENERATOR <<<\n";
        std::cout << "--------------------------------\n";
        std::cout << "Total Objects:          " << object_count << "\n";
        std::cout << "Raw Data Size:          " << total_raw << " bytes\n";
        std::cout << "Actual Disk Usage:      " << total_compressed << " bytes\n";
        std::cout << "Compression Efficiency: " << (int)savings << "%\n";
        std::cout << "--------------------------------\n";
    }

    bool is_ignored(const std::string& name){
        return (name[0] == '.' || name == ".mvc" || name == ".git" || name == "." || name == ".." || name == "mvc");
    }

}