#include "merge.h"
#include "utils.h"
#include "commit.h" 
#include "restore.h" 
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <queue>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

std::string to_hex(const std::string& bytes) {
    std::stringstream ss;
    for (unsigned char c : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
}

std::string clean_hash(std::string h) {
    while (!h.empty() && (isspace(h.back()) || h.back() == '\0')) h.pop_back();
    while (!h.empty() && (isspace(h.front()) || h.front() == '\0')) h.erase(0, 1);
    return h;
}

std::string get_parent_hash(const std::string& commit_hash) {
    std::string h = clean_hash(commit_hash);
    if (h.empty()) return "";
    std::string dir = h.substr(0, 2);
    std::string file = h.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    if (!fs::exists(path)) return "";
    
    std::string content = utils::decompress(utils::read_file(path));
    size_t parent_pos = content.find("\nparent ");
    if (parent_pos != std::string::npos) {
        size_t end = content.find('\n', parent_pos + 8);
        return clean_hash(content.substr(parent_pos + 8, end - (parent_pos + 8)));
    }
    return "";
}

// Need GE Engines, Delay expected (Avg HAL-LCA moment)
std::string find_lca(const std::string& commit1, const std::string& commit2) {
    //solution with the worst possible time complexity for finding LCA
    std::string c1 = clean_hash(commit1);
    std::string c2 = clean_hash(commit2);
    if (c1 == c2) return c1;
    
    std::set<std::string> ancestors1;
    std::queue<std::string> queue1;
    queue1.push(c1);
    ancestors1.insert(c1);

    int safety = 0;
    while (!queue1.empty() && safety < 5000) {
        std::string current = queue1.front();
        queue1.pop();
        std::string p = get_parent_hash(current);
        if (!p.empty()) {
            ancestors1.insert(p);
            queue1.push(p);
        }
        safety++;
    }

    std::string current = c2;
    safety = 0;
    while (!current.empty() && safety < 5000) {
        if (ancestors1.count(current)) return current;
        current = get_parent_hash(current);
        safety++;
    }
    return "";
}

void collect_files(const std::string& raw_tree_hash, const std::string& prefix, std::map<std::string, std::string>& file_map) {
    std::string tree_hash = clean_hash(raw_tree_hash);
    if (tree_hash.length() < 4) return;

    std::string dir = tree_hash.substr(0, 2);
    std::string file = tree_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return;

    std::string content = utils::decompress(utils::read_file(path));
    
    size_t pos = content.find('\0');
    if (pos == std::string::npos) return; 
    pos++; 

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
        std::string hex_hash = to_hex(raw_hash);
        pos += 20;

        if (mode == "40000") { 
            collect_files(hex_hash, prefix + name + "/", file_map);
        } else { 
            file_map[prefix + name] = hex_hash;
        }
    }
}

std::map<std::string, std::string> get_file_map(const std::string& raw_commit_hash) {
    std::map<std::string, std::string> files;
    std::string commit_hash = clean_hash(raw_commit_hash);
    if (commit_hash.empty()) return files;

    std::string dir = commit_hash.substr(0, 2);
    std::string file = commit_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return files;

    std::string content = utils::decompress(utils::read_file(path));
    
    size_t tree_pos = content.find("tree ");
    if (tree_pos != std::string::npos) {
        size_t end_line = content.find('\n', tree_pos);
        std::string tree_hash = content.substr(tree_pos + 5, end_line - (tree_pos + 5));
        collect_files(tree_hash, "", files);
    }
    return files;
}

bool merge_branch(const std::string& branch_name) {
    std::string target_hash, head_hash;

    std::string ref_path = ".mvc/refs/heads/" + branch_name;
    if (fs::exists(ref_path)) target_hash = clean_hash(utils::read_file(ref_path));
    else { std::cerr << "Error: Branch '" << branch_name << "' not found.\n"; return false; }

    std::string head_content = utils::read_file(".mvc/HEAD");
    std::string current_branch_ref = "";
    while(!head_content.empty() && isspace(head_content.back())) head_content.pop_back();

    if (head_content.rfind("ref: ", 0) == 0) {
        current_branch_ref = head_content;
        std::string h_path = ".mvc/" + head_content.substr(5);
        if (fs::exists(h_path)) head_hash = clean_hash(utils::read_file(h_path));
    } else {
        head_hash = clean_hash(head_content);
    }

    if (head_hash == target_hash) { std::cout << "Already up to date.\n"; return true; }

    std::string ancestor_hash = find_lca(head_hash, target_hash);
    
    if (ancestor_hash == head_hash) {
        std::cout << "Fast-forward merge...\n";
        restore(target_hash);
        if (!current_branch_ref.empty()) {
            std::string branch_file = ".mvc/" + current_branch_ref.substr(5);
            utils::write_file(branch_file, target_hash);
            utils::write_file(".mvc/HEAD", current_branch_ref);
        }
        return true;
    }

    std::cout << "Merging: Base=" << ancestor_hash.substr(0,7) 
              << " Head=" << head_hash.substr(0,7) 
              << " Target=" << target_hash.substr(0,7) << "\n";

    auto base_files = get_file_map(ancestor_hash);
    auto head_files = get_file_map(head_hash);
    auto target_files = get_file_map(target_hash);

    std::set<std::string> all_files;
    for (const auto& [f, h] : base_files) all_files.insert(f);
    for (const auto& [f, h] : head_files) all_files.insert(f);
    for (const auto& [f, h] : target_files) all_files.insert(f);

    bool has_conflict = false;

    for (const auto& file : all_files) {
        std::string h_base = base_files.count(file) ? base_files[file] : "";
        std::string h_head = head_files.count(file) ? head_files[file] : "";
        std::string h_target = target_files.count(file) ? target_files[file] : "";

        if (h_head == h_target) continue;

        if (h_head == h_base) {
            if (h_target.empty()) {
                std::cout << "Deleting: " << file << "\n";
                fs::remove(file);
            } else {
                std::cout << "Updating: " << file << "\n";
                std::string b_dir = h_target.substr(0, 2);
                std::string b_file = h_target.substr(2);
                std::string content = utils::decompress(utils::read_file(".mvc/objects/" + b_dir + "/" + b_file));
                size_t null_pos = content.find('\0');
                if (null_pos != std::string::npos) content = content.substr(null_pos + 1);
                
                if (fs::path(file).has_parent_path()) fs::create_directories(fs::path(file).parent_path());
                utils::write_file(file, content);
            }
        } 
        else if (h_target == h_base) { } 
        else {
            std::cerr << "CONFLICT (content): " << file << "\n";
            has_conflict = true;
        }
    }

    if (has_conflict) {
        std::cerr << "Merge aborted due to conflicts.\n";
        return false;
    }

    utils::write_file(".mvc/MERGE_HEAD", target_hash);
    std::cout << "Merge successful. updating index...\n";
    std::cout << "ACTION REQUIRED: Files merged.\n";
    std::cout << "Run: ./mvc commit -m \"Merge branch " << branch_name << "\"\n";
    return true;
}