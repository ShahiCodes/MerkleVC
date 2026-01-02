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
#include <fstream>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

std::map<std::string, std::string> get_files_from_commit(const std::string& commit_hash){
    std::map<std::string, std::string> file_map;
    if(commit_hash.empty()) return file_map;

    std::string dir = commit_hash.substr(0,2);
    std::string file = commit_hash.substr(2);
    std::string content = utils::decompress(utils::read_file(".mvc/objects/" + dir + "/" + file));

    size_t tree_pos = content.find("tree ");

    if(tree_pos == std::string::npos) return file_map;
    std::string tree_hash = content.substr(tree_pos + 5, 40);


    //partial implementation for graph verification
    // we need to refactor the restore.cpp to expose the tools
    return file_map;

}

std::string get_parent_hash(const std::string& commit_hash) {
    std::string dir = commit_hash.substr(0, 2);
    std::string file = commit_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return "";
    
    std::string content = utils::decompress(utils::read_file(path));
    
    size_t parent_pos = content.find("\nparent ");
    if (parent_pos != std::string::npos) {
        size_t end = content.find('\n', parent_pos + 8);
        return content.substr(parent_pos + 8, end - (parent_pos + 8));
    }
    return "";
}


// Need GE Engines, Delay expected (Avg HAL-LCA moment)
std::string find_lca(const std::string& commit1, const std::string& commit2) {
    //solution with the worst possible time complexity for finding LCA
    if (commit1 == commit2) return commit1;

    std::set<std::string> ancestors1;
    std::queue<std::string> queue1;
    queue1.push(commit1);
    ancestors1.insert(commit1);

    int safety = 0;
    while (!queue1.empty() && safety < 1000) {
        std::string current = queue1.front();
        queue1.pop();
        std::string p = get_parent_hash(current);
        if (!p.empty()) {
            ancestors1.insert(p);
            queue1.push(p);
        }
        safety++;
    }

    std::string current = commit2;
    safety = 0;
    while (!current.empty() && safety < 1000) {
        if (ancestors1.count(current)) {
            return current; 
        }
        current = get_parent_hash(current);
        safety++;
    }

    return "";
}


bool merge_branch(const std::string& branch_name) {
    std::string target_hash;
    std::string ref_path = ".mvc/refs/heads/" + branch_name;
    if (fs::exists(ref_path)) {
        target_hash = utils::read_file(ref_path);
        while (!target_hash.empty() && isspace(target_hash.back())) target_hash.pop_back();
    } else {
        std::cerr << "Error: Branch '" << branch_name << "' does not exist.\n";
        return false;
    }

    
    std::string head_hash;
    std::string head_content = utils::read_file(".mvc/HEAD");
    if (head_content.rfind("ref: ", 0) == 0) {
        std::string h_path = ".mvc/" + head_content.substr(5);
        while (!h_path.empty() && isspace(h_path.back())) h_path.pop_back();
        head_hash = utils::read_file(h_path);
    } else {
        head_hash = head_content;
    }
    while (!head_hash.empty() && isspace(head_hash.back())) head_hash.pop_back();

    if (head_hash == target_hash) {
        std::cout << "Already up to date.\n";
        return true;
    }

    
    std::string ancestor_hash = find_lca(head_hash, target_hash);
    if (ancestor_hash.empty()) {
        std::cerr << "Error: Unrelated histories (no common ancestor).\n";
        return false;
    }
    std::cout << "Merging: Base=" << ancestor_hash.substr(0,7) 
              << " Head=" << head_hash.substr(0,7) 
              << " Target=" << target_hash.substr(0,7) << "\n";


    if (ancestor_hash == head_hash) {
        std::cout << "Fast-forward merge...\n";

        std::string current_head_ref = "";
        std::string head_content_raw = utils::read_file(".mvc/HEAD");
        while (!head_content_raw.empty() && isspace(head_content_raw.back())) head_content_raw.pop_back();

        if (head_content_raw.rfind("ref: ", 0) == 0) {
            current_head_ref = head_content_raw;
        }

        if (!restore(target_hash)) {
            std::cerr << "Error: Failed to switch to target commit.\n";
            return false;
        }

        if (!current_head_ref.empty()) {
            std::cout << "Updating branch pointer for " << current_head_ref << "\n";
            
            std::string branch_file_path = ".mvc/" + current_head_ref.substr(5);
            utils::write_file(branch_file_path, target_hash);
            
            utils::write_file(".mvc/HEAD", current_head_ref);
        } else {
            std::cout << "Warning: You were in Detached HEAD state. HEAD updated but no branch moved.\n";
        }
        
        return true;
    }

    std::cout << "Merging: Base=" << ancestor_hash.substr(0,7) 
              << " + " << target_hash.substr(0,7) << "\n";

    std::cout << "Performing 3-Way Merge (Strategy: Ours - keeping current files, linking history)...\n";

    std::string head_commit_content = utils::decompress(utils::read_file(".mvc/objects/" + head_hash.substr(0,2) + "/" + head_hash.substr(2)));
    std::string tree_hash = head_commit_content.substr(5, 40); 

    // Create the Merge Commit
    std::vector<std::string> parents = {head_hash, target_hash};
    std::string msg = "Merge branch '" + branch_name + "'";
    
    std::string new_commit = commit_tree(tree_hash, msg, parents);
    
    std::cout << "Merge complete! New commit: " << new_commit.substr(0,7) << "\n";
    return true;

    // OLD
    // std::cerr << "True Recursive 3-Way Merge is complex. I am not implementing that make peace with it.\n";
    // std::cerr << "Falling back to safe default: Please checkout the other branch manually.\n";
    // std::cerr << "Code is doomed. :( \n";
    // return false;
}