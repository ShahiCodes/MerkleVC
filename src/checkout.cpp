#include "checkout.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

std::string bytes_to_hex(const std::string& bytes){
    std::stringstream ss;
    for (unsigned char c : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
}

void restore_tree(const std::string& tree_hash, const std::string& current_path);

void restore_blob(const std::string& hash, const std::string& path){
    std::string dir = hash.substr(0,2);
    std::string file = hash.substr(2);
    std::string object_path = ".mvc/objects/" + dir + "/" + file;

    if(!fs::exists(object_path)){
        std::cerr << "Error: Blob " << hash << "not found, missing." ;
        return;
    }

    std::string compressed = utils::read_file(object_path);
    std::string raw = utils::decompress(compressed);

    size_t null_pos = raw.find('\0');
    if(null_pos == std::string::npos) return;

    std::string content = raw.substr(null_pos + 1);

    utils::write_file(path, content);

    // DEBUG LINE
    std::cout << "DEBUG: Restored blob " << hash << " to " << path << std::endl;
}

void restore_tree(const std::string& tree_hash, const std::string& current_path){
    std::string dir = tree_hash.substr(0, 2);
    std::string file = tree_hash.substr(2);
    std::string object_path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(object_path)) {
        std::cerr << "Error: Tree " << tree_hash << " missing.\n";
        return;
    }

    std::string compressed = utils::read_file(object_path);
    std::string raw = utils::decompress(compressed);

    size_t null_pos = raw.find('\0');
    if(null_pos == std::string::npos) return;

    std::string body = raw.substr(null_pos + 1);

    // Format: [mode] [name]\0[20-byte-hash]
    size_t cursor = 0;
    while(cursor < body.size()){
        // Read mode (until space)
        size_t space_pos = body.find(' ', cursor);
        if(space_pos == std::string::npos) break;
        std::string mode = body.substr(cursor, space_pos-cursor);
        cursor = space_pos + 1;

        // Read name (until null)
        size_t null_pos = body.find('\0', cursor);
        if(null_pos == std::string::npos) break;
        std::string name = body.substr(cursor, null_pos-cursor);
        cursor = null_pos + 1;

        // Read Hash (20 bytes)
        std::string raw_hash = body.substr(cursor, 20);
        std::string hash_hex = bytes_to_hex(raw_hash);
        cursor += 20;

        std::string full_path = current_path;
        if(!full_path.empty()) full_path += "/";
        full_path += name;

        if(mode == "40000"){
            // Directory
            fs::create_directories(full_path);
            restore_tree(hash_hex, full_path);
        }
        else{
            // File
            restore_blob(hash_hex, full_path);
        }

    }
}

void checkout(const std::string& commit_hash){
    std::string dir = commit_hash.substr(0,2);
    std::string file = commit_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;

    if(!fs::exists(path)){
        std::cerr << "Error: Commit " << commit_hash << " not found.\n";
        return ;
    }

    std::string compressed = utils::read_file(path);
    std::string raw = utils::decompress(compressed);

    size_t tree_pos = raw.find("tree ");
    if(tree_pos == std::string::npos){
        std::cerr << "Error: Invalid commit object (no Tree found).\n";
        return;
    }

    size_t hash_start = tree_pos + 5;
    std::string tree_hash = raw.substr(hash_start, 40);

    std::cout << "Checking out Tree: " << tree_hash << std::endl;

    restore_tree(tree_hash, "");
    std::cout << "Done \n";
}