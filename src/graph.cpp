#include "graph.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;


std::string get_graph_parent(const std::string& content) {
    size_t parent_pos = content.find("\nparent ");
    if (parent_pos != std::string::npos) {
        size_t end = content.find('\n', parent_pos + 8);
        return content.substr(parent_pos + 8, end - (parent_pos + 8));
    }
    return ""; 
}

std::vector<std::string> get_branches_for_commit(const std::string& hash) {
    std::vector<std::string> branches;
    
    
    std::string head_content = utils::read_file(".mvc/HEAD");
    while (!head_content.empty() && isspace(head_content.back())) head_content.pop_back();
    
    std::string current_branch = "";
    bool is_detached = true;
    if (head_content.rfind("ref: refs/heads/", 0) == 0) {
        current_branch = head_content.substr(16); 
        is_detached = false;
    }

    if (fs::exists(".mvc/refs/heads")) {
        for (const auto& entry : fs::directory_iterator(".mvc/refs/heads")) {
            std::string branch_name = entry.path().filename().string();
            std::string branch_hash = utils::read_file(entry.path().string());
            while (!branch_hash.empty() && isspace(branch_hash.back())) branch_hash.pop_back();
            
            if (branch_hash == hash) {
                if (!is_detached && branch_name == current_branch) {
                    branches.push_back(branch_name + " \033[1;36m-> HEAD\033[0m"); 
                } else {
                    branches.push_back(branch_name);
                }
            }
        }
    }

    if (is_detached && head_content == hash) {
        branches.push_back("\033[1;36mHEAD\033[0m");
    }
    
    return branches;
}

void print_subtree(const std::string& current_hash, 
                   const std::map<std::string, std::vector<std::string>>& adj,
                   const std::string& prefix, 
                   bool is_last) {
    
    std::string marker = is_last ? "└── " : "├── ";
    
    std::vector<std::string> branches = get_branches_for_commit(current_hash);
    
    std::cout << prefix << marker << "\033[33m" << current_hash.substr(0, 7) << "\033[0m"; // Yellow Hash
    
    if (!branches.empty()) {
        std::cout << " (";
        for (size_t i = 0; i < branches.size(); ++i) {
            if (branches[i].find("HEAD") != std::string::npos) {

                 std::cout << "\033[32m" << branches[i] << "\033[0m";
            } else {
                std::cout << "\033[32m" << branches[i] << "\033[0m"; 
            }
            
            if (i < branches.size() - 1) std::cout << ", ";
        }
        std::cout << ")";
    }
    std::cout << "\n";

    std::string child_prefix = prefix + (is_last ? "    " : "│   ");

    if (adj.count(current_hash)) {
        const auto& children = adj.at(current_hash);
        for (size_t i = 0; i < children.size(); ++i) {
            print_subtree(children[i], adj, child_prefix, i == children.size() - 1);
        }
    }
}

void show_graph() {
    std::map<std::string, std::vector<std::string>> adj; 
    std::set<std::string> roots; 
    std::set<std::string> all_commits;

    for (const auto& entry : fs::recursive_directory_iterator(".mvc/objects")) {
        if (fs::is_regular_file(entry)) {
            std::string hash = entry.path().parent_path().filename().string() + 
                               entry.path().filename().string();
            try {
                std::string content = utils::decompress(utils::read_file(entry.path().string()));
                if (content.rfind("commit ", 0) != std::string::npos) {
                    size_t null_pos = content.find('\0');
                    if (null_pos != std::string::npos) content = content.substr(null_pos + 1);

                    all_commits.insert(hash);
                    std::string parent = get_graph_parent(content);
                    while (!parent.empty() && isspace(parent.back())) parent.pop_back();

                    if (parent.empty()) {
                        roots.insert(hash);
                    } else {
                        adj[parent].push_back(hash);
                    }
                }
            } catch (...) { continue; }
        }
    }

    if (roots.empty()) {
        std::cerr << "No commits found.\n";
        return;
    }

    std::cout << "Repository Graph:\n";
    for (const auto& root : roots) {
        print_subtree(root, adj, "", true);
        std::cout << "\n";
    }
}