#include "status.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

std::string to_hex_status(const std::string& bytes){
    std::stringstream ss;
    for(unsigned char c : bytes){
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
}

void collect_head_files(const std::string& raw_tree_hash, const std::string& prefix, std::map<std::string, std::string>& file_map){
    std:: string tree_hash = raw_tree_hash;
    while (!tree_hash.empty() && (isspace(tree_hash.back()) || tree_hash.back() == '\0')) tree_hash.pop_back();
    while (!tree_hash.empty() && (isspace(tree_hash.front()) || tree_hash.front() == '\0')) tree_hash.erase(0, 1);

    if (tree_hash.length() < 4) return;

    std::string dir = tree_hash.substr(0, 2);
    std::string file = tree_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return;
    std::string content = utils::decompress(utils::read_file(path));
    
    // skip Header
    size_t pos = content.find('\0');
    if (pos == std::string::npos) return; 
    pos++;
    // Parse Binary Entries
    while (pos < content.size()) {
        size_t space_pos = content.find(' ', pos);
        if (space_pos == std::string::npos) break;
        std::string mode = content.substr(pos, space_pos - pos);
        pos = space_pos + 1;

        size_t null_pos = content.find('\0', pos);
        if (null_pos == std::string::npos) break;
        std::string name = content.substr(pos, null_pos - pos);
        pos = null_pos + 1;

        if (pos + 20 > content.size()) break;
        std::string raw_hash = content.substr(pos, 20);
        std::string hex_hash = to_hex_status(raw_hash);
        pos += 20;

        if (mode == "40000") { 
            collect_head_files(hex_hash, prefix + name + "/", file_map);
        } else { 
            file_map[prefix + name] = hex_hash;
        }
    }
}

std::map<std::string, std::string> get_head_state() {
    std::map<std::string, std::string> files;
    
    // get HEAD commit hsh
    std::string head_content = utils::read_file(".mvc/HEAD");
    while(!head_content.empty() && isspace(head_content.back())) head_content.pop_back();
    
    std::string commit_hash;
    if (head_content.rfind("ref: ", 0) == 0) {
        std::string ref_path = ".mvc/" + head_content.substr(5);
        if (fs::exists(ref_path)) commit_hash = utils::read_file(ref_path);
    } else {
        commit_hash = head_content;
    }
    
    
    while (!commit_hash.empty() && (isspace(commit_hash.back()) || commit_hash.back() == '\0')) commit_hash.pop_back();

    if (commit_hash.empty()) return files; // Empty repo

    // tree hash from commit
    std::string dir = commit_hash.substr(0, 2);
    std::string file = commit_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return files;

    std::string content = utils::decompress(utils::read_file(path));
    size_t tree_pos = content.find("tree ");
    if (tree_pos != std::string::npos) {
        size_t end_line = content.find('\n', tree_pos);
        std::string tree_hash = content.substr(tree_pos + 5, end_line - (tree_pos + 5));
        collect_head_files(tree_hash, "", files);
    }
    return files;
}

void scan_disk(const std::string& current_path, std::map<std::string, std::string>& disk_files) {
    for (const auto& entry : fs::directory_iterator(current_path)) {
        std::string path = entry.path().string();
        std::string name = entry.path().filename().string();
        
        if (name == ".mvc" || name == ".git" || name == "." || name == "..") continue;
        if (name == "mvc") continue;

        if (fs::is_directory(entry)) {
            scan_disk(path, disk_files);
        } else {
            std::string relative_path = path;
            if (relative_path.rfind("./", 0) == 0) relative_path = relative_path.substr(2);
            
            std::string content = utils::read_file(path);
            std::string hash = utils::sha1(content);
            // utils::sha1 usually hashes raw string. 
            // Git hashes "blob size\0content", for status to match repo.cpp, replicating the header logic.
            std::string header = "blob " + std::to_string(content.size()) + '\0';
            std::string full_data = header + content;
            std::string git_hash = utils::sha1(full_data);

            disk_files[relative_path] = git_hash;
        }
    }
}


void show_status() {
    std::map<std::string, std::string> head_files = get_head_state();
    
    std::map<std::string, std::string> disk_files;
    scan_disk(".", disk_files);

    std::vector<std::string> modified;
    std::vector<std::string> deleted;
    std::vector<std::string> untracked;

    for (const auto& [path, disk_hash] : disk_files) {
        if (head_files.count(path)) {
            if (head_files[path] != disk_hash) {
                modified.push_back(path);
            }
            head_files.erase(path);
        } else {
            untracked.push_back(path);
        }
    }

    for (const auto& [path, hash] : head_files) {
        deleted.push_back(path);
    }

    std::string head_content = utils::read_file(".mvc/HEAD");
    std::string current_branch = "DETACHED HEAD";
    if (head_content.rfind("ref: refs/heads/", 0) == 0) {
        current_branch = head_content.substr(16);
        while(!current_branch.empty() && isspace(current_branch.back())) current_branch.pop_back();
    }
    
    std::cout << "On branch \033[1;36m" << current_branch << "\033[0m\n\n";

    if (modified.empty() && deleted.empty() && untracked.empty()) {
        std::cout << "nothing to commit, working tree clean\n";
        return;
    }

    if (!modified.empty() || !deleted.empty()) {
        std::cout << "Changes not staged for commit:\n";
        for (const auto& f : modified) std::cout << "    \033[31mmodified:   " << f << "\033[0m\n";
        for (const auto& f : deleted)  std::cout << "    \033[31mdeleted:    " << f << "\033[0m\n";
        std::cout << "\n";
    }

    if (!untracked.empty()) {
        std::cout << "Untracked files:\n";
        for (const auto& f : untracked) std::cout << "    \033[31m" << f << "\033[0m\n";
        std::cout << "\n";
    }
}