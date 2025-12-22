#include "commit.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

std::string get_currrent_time(){
    std::time_t now = std::time(nullptr);
    return std::to_string(now) + " +0000";
}

std::string get_head_ref_path() {
    std::string head_content = utils::read_file(".mvc/HEAD");

    if(!head_content.empty() && head_content.back() == '\n'){
        head_content.pop_back();
    }

    std::string prefix = "ref: ";
    if(head_content.rfind(prefix, 0) == 0){
        return ".mvc/" + head_content.substr(prefix.size());
    }
    return "";  
}

std::string get_parent_commit() {
    // 1. Read .mvc/HEAD
    std::string head_content = utils::read_file(".mvc/HEAD");
    if (head_content.empty()) return "";
    
    if (head_content.back() == '\n') head_content.pop_back();

    // 2. Case A: It's a Branch (e.g., "ref: refs/heads/master")
    if (head_content.rfind("ref: ", 0) == 0) {
        std::string ref_path = ".mvc/" + head_content.substr(5);
        if (fs::exists(ref_path)) {
            std::string hash = utils::read_file(ref_path);
            if (!hash.empty() && hash.back() == '\n') hash.pop_back();
            return hash;
        }
    } 
    // 3. Case B: It's a Detached Hash (e.g., "9acfa3...")
    else {
        return head_content;
    }

    return "";
}

void update_head(const std::string& commit_hash) {
    std::string ref_path = get_head_ref_path();
    
    if (ref_path.empty()) {
        // Case 1: Detached HEAD (We are not on a branch)
        // Just update HEAD to point to the new commit directly.
        utils::write_file(".mvc/HEAD", commit_hash);
    } else {
        // Case 2: Attached HEAD (We are on a branch like master)
        // Update the branch pointer file (e.g., .mvc/refs/heads/master)
        fs::create_directories(fs::path(ref_path).parent_path());
        
        std::ofstream file(ref_path);
        file << commit_hash << "\n";
    }
}

std::string commit_tree(const std::string& tree_sha, const std::string& message){
    std::stringstream commit_content;

    commit_content << "tree " << tree_sha << "\n";

    std::string parent = get_parent_commit();
    if(!parent.empty()){
        commit_content << "parent " << parent << "\n";
    }

    std::string author = "Username user@example.com";
    std::string timestamp = get_currrent_time();

    commit_content << "author " << author << " " << timestamp << "\n";
    commit_content << "committer " << author << " " << timestamp << "\n\n";

    commit_content << message << "\n";

    std::string data = commit_content.str();
    std::string header = "commit " + std::to_string(data.size()) + '\0';
    std::string store_data = header + data;

    std::string commit_sha1 = utils::sha1(store_data);

    std::string dir_name = commit_sha1.substr(0,2);
    std::string file_name = commit_sha1.substr(2);
    utils::write_file(".mvc/objects/" + dir_name + "/" + file_name, utils::compress(store_data));

    update_head(commit_sha1);
    return commit_sha1;

}

