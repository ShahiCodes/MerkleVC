#include "restore.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <set>
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

void collect_files_in_tree(const std::string& tree_hash, const std::string& current_path, std::set<std::string>& files) {
    std::string dir = tree_hash.substr(0, 2);
    std::string file = tree_hash.substr(2);
    std::string object_path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(object_path)) return;

    std::string compressed = utils::read_file(object_path);
    std::string raw = utils::decompress(compressed);
    
    // Strip header
    size_t null_pos = raw.find('\0');
    if (null_pos == std::string::npos) return;
    std::string body = raw.substr(null_pos + 1);
    
    size_t cursor = 0;
    while (cursor < body.size()) {
        // Mode
        size_t space_pos = body.find(' ', cursor);
        if (space_pos == std::string::npos) break;
        std::string mode = body.substr(cursor, space_pos - cursor);
        cursor = space_pos + 1;
        
        // Name
        size_t null_byte = body.find('\0', cursor);
        if (null_byte == std::string::npos) break;
        std::string name = body.substr(cursor, null_byte - cursor);
        cursor = null_byte + 1;
        
        // Hash
        std::string raw_hash = body.substr(cursor, 20);
        std::string hash_hex = bytes_to_hex(raw_hash);
        cursor += 20;
        
        // Path
        std::string full_path = current_path.empty() ? name : current_path + "/" + name;
        
        if (mode == "40000") {
            // Recurse into directory
            collect_files_in_tree(hash_hex, full_path, files);
        } else {
            // Record file path
            files.insert(full_path);
        }
    }
}

std::string get_tree_hash_from_commit(const std::string& commit_hash) {
    std::string dir = commit_hash.substr(0, 2);
    std::string file = commit_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return "";
    
    std::string compressed = utils::read_file(path);
    std::string raw = utils::decompress(compressed);
    
    size_t tree_pos = raw.find("tree ");
    if (tree_pos == std::string::npos) return "";
    
    return raw.substr(tree_pos + 5, 40);
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

bool restore(const std::string& target_commit_hash){

    std::string target_tree_hash = get_tree_hash_from_commit(target_commit_hash);
    if (target_tree_hash.empty()) {
        std::cerr << "Error: Invalid target commit.\n";
        return false;
    }
    std::set<std::string> head_files;

    std::string head_content = utils::read_file(".mvc/HEAD");
    if (!head_content.empty()) {
        if (head_content.back() == '\n') head_content.pop_back();
        std::string current_commit_hash;
        
        // Handle Refs vs Detached
        if (head_content.rfind("ref: ", 0) == 0) {
            std::string ref_path = ".mvc/" + head_content.substr(5);
            if (fs::exists(ref_path)) {
                current_commit_hash = utils::read_file(ref_path);
                if (!current_commit_hash.empty() && current_commit_hash.back() == '\n') 
                    current_commit_hash.pop_back();
            }
        } else {
            current_commit_hash = head_content;
        }
        if (!current_commit_hash.empty()) {
            std::string current_tree_hash = get_tree_hash_from_commit(current_commit_hash);
            if (!current_tree_hash.empty()) {
                collect_files_in_tree(current_tree_hash, "", head_files);
            }
        }
    }

    std::set<std::string> target_files;
    collect_files_in_tree(target_tree_hash, "", target_files);

    // Remove files that are in HEAD but not in Target
    for (const auto& file : head_files) {
        if (target_files.find(file) == target_files.end()) {
            if (fs::exists(file)) {
                std::cout << "Deleting stale file: " << file << "\n";
                fs::remove(file);
            }
        }
    }

    std::cout << "Restoring Tree: " << target_tree_hash << std::endl;

    restore_tree(target_tree_hash, "");
    utils::write_file(".mvc/HEAD", target_commit_hash);
    std::cout << "Done \n";

    return true;
}