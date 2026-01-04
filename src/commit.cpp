#include "commit.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

std::string get_current_time() {
    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%s %z", std::localtime(&now)); 
    return std::string(buf);
}

void append_to_global_log(const std::string& hash, const std::string& message, std::string author){
    std::ofstream log_file(".mvc/global_log.txt", std::ios::app);
    if(log_file.is_open()){
        std::time_t now = std::time(nullptr);
        char time_buf[100];
        std::strftime(time_buf, sizeof(time_buf),"%Y-%m-%d %H:%M:%S", std::localtime(&now));

        log_file << "[" << time_buf << "] " << hash << " - " << message << " by - " << author << "\n";
        log_file.close();
    }
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
    
    std::string head_content = utils::read_file(".mvc/HEAD");
    if (head_content.empty()) return "";
    
    if (head_content.back() == '\n') head_content.pop_back();

    if (head_content.rfind("ref: ", 0) == 0) {
        std::string ref_path = ".mvc/" + head_content.substr(5);
        if (fs::exists(ref_path)) {
            std::string hash = utils::read_file(ref_path);
            if (!hash.empty() && hash.back() == '\n') hash.pop_back();
            return hash;
        }
    } 
    else {
        return head_content;
    }

    return "";
}

void update_head(const std::string& commit_hash) {
    std::string ref_path = get_head_ref_path();
    
    if (ref_path.empty()) {
        utils::write_file(".mvc/HEAD", commit_hash);
    } else {

        fs::create_directories(fs::path(ref_path).parent_path());
        
        std::ofstream file(ref_path);
        file << commit_hash << "\n";
    }
}

std::string commit_tree(const std::string& tree_hash,
    const std::string& message,
    const std::vector<std::string>& explicit_parent) {

    std::vector<std::string> parents = explicit_parent;
    
    if (parents.empty()) {
        // A. Primary Parent (HEAD)
        std::string head_content = utils::read_file(".mvc/HEAD");
        std::string parent_hash = "";
        
        if (head_content.rfind("ref: ", 0) == 0) {
            std::string ref_path = ".mvc/" + head_content.substr(5);
            if (head_content.back() == '\n') ref_path.pop_back();
            if (std::filesystem::exists(ref_path)) {
                parent_hash = utils::read_file(ref_path);
            }
        } else {
            parent_hash = head_content;
        }
        
        while (!parent_hash.empty() && isspace(parent_hash.back())) parent_hash.pop_back();
        if (!parent_hash.empty()) {
            parents.push_back(parent_hash);
        }

        if (std::filesystem::exists(".mvc/MERGE_HEAD")) {
            std::string merge_head = utils::read_file(".mvc/MERGE_HEAD");
            while (!merge_head.empty() && isspace(merge_head.back())) merge_head.pop_back();
            if (!merge_head.empty()) {
                parents.push_back(merge_head);
            }
            std::filesystem::remove(".mvc/MERGE_HEAD");
        }
    }


    std::string author;
    if(fs::exists(".mvc/config")){
        author = utils::read_file(".mvc/config");
        while (!author.empty() && isspace(author.back())) author.pop_back();
    }
    else{
        author = "user user@example.com";
    }

    std::string timestamp = get_current_time();

    std::stringstream ss;
    ss << "tree " << tree_hash  << "\n";

    for(const auto& p: parents){
        ss << "parent " << p << "\n";
    }

    ss << "author " << author << " " << timestamp << "\n";
    ss << "committer " << author << " " << timestamp << "\n\n";
    ss << message << "\n";

    std::string content = ss.str();
    std::string header = "commit " + std::to_string(content.size()) + '\0';
    std::string store_data = header + content;

    std::string commit_hash = utils::sha1(store_data);
    std::string dir = commit_hash.substr(0, 2);
    std::string file = commit_hash.substr(2);

    if (!std::filesystem::exists(".mvc/objects/" + dir)) {
        std::filesystem::create_directories(".mvc/objects/" + dir);
    }
    utils::write_file(".mvc/objects/" + dir + "/" + file, utils::compress(store_data));

    std::string head_content = utils::read_file(".mvc/HEAD");
    if(head_content.rfind("ref: ", 0) == 0){
        std::string ref_path = ".mvc/" + head_content.substr(5);
        if(head_content.back() == '\n') head_content.pop_back();

        utils::write_file(ref_path, commit_hash);
    }
    else{
        utils::write_file(".mvc/HEAD", commit_hash);
    }
    append_to_global_log(commit_hash, message, author);
    return commit_hash;
}

