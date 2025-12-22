#include "repository.h"
#include "utils.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

bool is_ignored(const std::string& name){
    return (name == ".mvc" || name == ".git" || name == "." || name == ".." || name == "mvc");
}

std::string store_blob(const std::string& path) {
    std::string content = utils::read_file(path);
    std::string header = "blob " + std::to_string(content.size()) + '\0';
    std::string store_data = header + content;
    std::string sha1_hash = utils::sha1(store_data);
    std::string compressed_data = utils::compress(store_data);
    
    std::string dir_name = sha1_hash.substr(0, 2);
    std::string file_name = sha1_hash.substr(2);
    std::string object_path = ".mvc/objects/" + dir_name + "/" + file_name;
    
    utils::write_file(object_path, compressed_data);
    return sha1_hash;
}

struct TreeEntry {
    std::string name;
    std::string mode; // "100644" for files, "40000" for dirs
    std::string hash;
};

std::string write_tree(const std::string& path){
    std::vector<TreeEntry> entries;
    for(const auto& entry : fs::directory_iterator(path)){
        std::string name = entry.path().filename().string();

        if(is_ignored(name)){
            continue;
        }

        TreeEntry tree_entry;
        tree_entry.name = name;

        if(entry.is_directory()){
            tree_entry.mode = "40000";
            tree_entry.hash = write_tree(entry.path().string());
        }
        else{
            tree_entry.mode = "100644";
            tree_entry.hash = store_blob(entry.path().string());
        }

        entries.push_back(tree_entry);
        
        // debug statement
        std::cout << "DEBUG: Added " << name << " (" << tree_entry.mode << ")\n";
    }

    std::sort(entries.begin(), entries.end(), [](const TreeEntry& a, const TreeEntry& b){
        return a.name < b.name;
    });

    std::string tree_body;
    for(const auto& e : entries){
        tree_body += e.mode + " " + e.name + '\0' + utils::hex_to_bytes(e.hash);
    }

    std::string header = "tree " + std::to_string(tree_body.size()) + '\0';
    std::string store_data = header + tree_body;
    std::string tree_sha1 = utils::sha1(store_data);

    std::string dir_name = tree_sha1.substr(0,2);
    std::string file_name = tree_sha1.substr(2);
    std::string object_path = ".mvc/objects/" + dir_name + "/" + file_name;

    utils::write_file(object_path, utils::compress(store_data));
    return tree_sha1;
}

std::string get_tree_from_commit(const std::string& commit_hash){
    if(commit_hash.empty()){
        return "";
    }

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

bool is_work_tree_clean(){
    std::string current_root_hash = write_tree(".");
    std::string head_content = utils::read_file(".mvc/HEAD");
    if(head_content.empty()) return true;

    if(head_content.back() == '\n'){
        head_content.pop_back();
    }

    std::string head_commit_hash;
    if (head_content.rfind("ref: ", 0) == 0){
        std::string ref_path = ".mvc/" + head_content.substr(5);
        if(fs::exists(ref_path)){
            head_commit_hash = utils::read_file(ref_path);
            if(!head_commit_hash.empty() && head_commit_hash.back() == '\n'){
                head_commit_hash.pop_back();
            }
        }
    }
    else{
        head_commit_hash = head_content;
    }

    if(head_commit_hash.empty()){
        return current_root_hash.empty();
    }

    std::string head_tree_hash = get_tree_from_commit(head_commit_hash);
    return current_root_hash == head_tree_hash;
}


bool init_repository() {
    const std::string root_dir = ".mvc";

    if(fs::exists(root_dir)) {
        std::cerr << "Respository already exists. \n";
        return false;
    }

    try{
        if(fs::create_directories(root_dir)){
            fs::create_directories(root_dir + "/objects");
            fs::create_directories(root_dir + "/refs");

            std::ofstream head_file(root_dir + "/HEAD");
            if(head_file.is_open()){
                head_file << "ref: refs/heads/master\n";
                head_file.close();
            }
            else{
                std::cerr << "Error: couldn't create HEAD file. \n";
                return false;
            }

            std::cout << "Initialized empty MerkleVC repository in " << fs::absolute(root_dir) << "\n";
            return true;
        }
        else{
            std::cerr << "Error: failed to create .mvc directory \n";
            return false;
        }
    }
    catch(const fs::filesystem_error& e){
        std::cerr << "Filesystem error: " << e.what() << "\n";
        return false;
    }

}