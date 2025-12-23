#include "branch.h"
#include "utils.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype> 

namespace fs = std::filesystem;

// Helper to remove trailing newlines and spaces
void trim_right(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
}

std::string get_current_head_hash() {
    std::string head_content = utils::read_file(".mvc/HEAD");
    if (head_content.empty()) return "";
    
    trim_right(head_content); 
    if (head_content.rfind("ref: ", 0) == 0) {
        std::string ref_path = ".mvc/" + head_content.substr(5);
        trim_right(ref_path); 

        if (fs::exists(ref_path)) {
            std::string hash = utils::read_file(ref_path);
            trim_right(hash);
            return hash;
        } else {
            std::cerr << "Debug: Expected ref file at '" << ref_path << "' but it is missing.\n";
            return "";
        }
    } 
    else {
        return head_content;
    }
}

bool create_branch(const std::string& branch_name) {
    if (branch_name.empty() || branch_name.find(' ') != std::string::npos) {
        std::cerr << "Error: Invalid branch name.\n";
        return false;
    }

    std::string branch_path = ".mvc/refs/heads/" + branch_name;
    if (fs::exists(branch_path)) {
        std::cerr << "Error: Branch '" << branch_name << "' already exists.\n";
        return false;
    }

    std::string current_hash = get_current_head_hash();
    if (current_hash.empty()) {
        std::cerr << "Error: Not a valid object name: 'HEAD'. (Make a commit first?)\n";
        return false;
    }

    fs::create_directories(".mvc/refs/heads");
    utils::write_file(branch_path, current_hash);
    
    std::cout << "Created branch '" << branch_name << "' at " << current_hash.substr(0, 7) << "\n";
    return true;
}

std::vector<std::string> list_branches() {
    std::vector<std::string> branches;
    std::string path = ".mvc/refs/heads";
    
    if (fs::exists(path)) {
        for (const auto& entry : fs::directory_iterator(path)) {
            branches.push_back(entry.path().filename().string());
        }
    }
    return branches;
}

bool delete_branch(const std::string& branch_name) {
    std::string branch_path = ".mvc/refs/heads/" + branch_name;
    if (!fs::exists(branch_path)) {
        std::cerr << "Error: Branch '" << branch_name << "' not found.\n";
        return false;
    }

    std::string head_content = utils::read_file(".mvc/HEAD");
    trim_right(head_content);
    
    if (head_content == "ref: refs/heads/" + branch_name) {
        std::cerr << "Error: Cannot delete the branch you are currently on.\n";
        return false;
    }

    try {
        fs::remove(branch_path);
        std::cout << "Deleted branch " << branch_name << ".\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: Could not delete branch. " << e.what() << "\n";
        return false;
    }
}