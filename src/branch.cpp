#include "branch.h"
#include "utils.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

std::string get_current_head_hash() {
    std::string head_content = utils::read_file(".mvc/HEAD");

    if(head_content.empty()){
        return "";
    }

    if(head_content.back() == '\n'){
        head_content.pop_back();
    }

    if(head_content.rfind("ref: ", 0) == 0){
        std::string ref_path = "./mvc" + head_content.substr(5);
        if(fs::exists(ref_path)){
            std::string hash = utils::read_file(ref_path);
            if(!hash.empty() && hash.back() == '\n'){
                hash.pop_back();
            }
            return hash;
        }
    }
    else{
        return head_content;
    }
    return "";   
}

bool create_branch (const std::string& branch_name){
    if(branch_name.empty() || branch_name.find(' ') != std::string::npos){
        std::cerr << "Error: Invalid branch name.\n";
        return false;
    }
    std::string branch_path = ".mvc/refs/heads/" + branch_name;
    if(fs::exists(branch_path)){
        std::cerr << "Error: Branch ' " << branch_name << "' already exists.\n";
        return false;
    }

    std::string current_hash = get_current_head_hash();
    if(current_hash.empty()){
        std::cerr << "Error: Not a valid object name: 'HEAD'. (Make a commit first?)\n";
        return false;
    }

    fs::create_directories(".mvc/refs/heads");
    utils::write_file(branch_path, current_hash + "\n");

    std::cout << "Branch '" << branch_name << "' created at " << current_hash << "\n";
    return true;
}

std::vector<std::string> list_branches(){
    std::vector<std::string> branches;
    std::string path = ".mvc/refs/heads";

    if(fs::exists(path)){
        for(const auto& entry: fs::directory_iterator(path)){
            branches.push_back(entry.path().filename().string());
        }
    }
    return branches;
}