#include "repository.h"
#include "utils.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <map>
#include <string>

namespace fs = std::filesystem;

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

//to remove trailing slash like src/ -> src
std::string clean(std::string p) {
    if(p.find("./") == 0) p = p.substr(2);
    while(!p.empty() && p.back() == '/') p.pop_back(); 
    return p;
}

//Returns false if the file we are checking was not staged by the user
bool check(fs::path current_path, const std::vector<fs::path>& added_paths)
{
    std::string curr_path = clean(current_path.string());
    
    for(auto& v : added_paths)
    {
        std::string cp_v = v.string();
        cp_v = clean(cp_v);   

        //exact match
        if(cp_v == curr_path) return true; 
        //if src/ was added using git add, and then it should match with src/main.cpp
        else if(cp_v.size() < curr_path.size() && curr_path.find(cp_v + "/") == 0) return true; 
        //if include/main.cpp was added and then we are at include/ so we must enter that directory
        else if(cp_v.size() > curr_path.size() && cp_v.find(curr_path + "/") == 0) return true;
    }
    return false;
}

std::string build(fs::path path, const std::vector<fs::path>& added_paths)
{
    std::vector<TreeEntry> entries;

    for(const auto& entry : fs::directory_iterator(path)){
        // directory_iterator iterates over entries in the given directory (non-recursive).
            // it doesn't open subdirectories
        std::string name = entry.path().filename().string();

        if(utils::is_ignored(name)){
            continue;
        }

        if(check(entry, added_paths) == false) continue;

        TreeEntry tree_entry;
        tree_entry.name = name;

        if(entry.is_directory()){
            tree_entry.mode = "40000";
            fs::path dir_path = entry;

            // std::cout << "DEBUGGGGG: directory committed: " << dir_path << '\n';

            tree_entry.hash = build(dir_path, added_paths);
        }
        else{
            tree_entry.mode = "100644";

            // std::cout << "DEBUGGGGG: file committed: " << entry.path().string() << '\n';

            tree_entry.hash = store_blob(entry.path().string());
        }

        entries.push_back(tree_entry);
        
        // debug statement
        // std::cout << "DEBUG: Added " << name << " (" << tree_entry.mode << ")\n";
    }


    std::sort(entries.begin(), entries.end(), [](const TreeEntry& a, const TreeEntry& b){
        return a.name < b.name;
    });
     //sorting is necessary here to ensure that same tree structure is made for the same input everytime
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

std::string write_tree(const std::vector<fs::path>& added_paths){
    return build(".", added_paths);
}

//every file is stored in the object directory, only the latest commit hash of a  branch is stored in the 
			// ref/heads/branch_name file.
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

// Helper: Recursive function to check if files in a tree match the disk
bool validate_tree(const std::string& tree_hash, const std::string& prefix, const std::vector<fs::path>& tracked_files) {
    if (tree_hash.empty()) return true;

    std::string dir = tree_hash.substr(0, 2);
    std::string file = tree_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    if (!fs::exists(path)) return false; 

    std::string content = utils::decompress(utils::read_file(path));
    size_t pos = content.find('\0'); 
    if (pos == std::string::npos) return true; 
    pos++; 

    while (pos < content.size()) {
        size_t space = content.find(' ', pos);
        if (space == std::string::npos) break;
        
        std::string mode = content.substr(pos, space - pos);
        pos = space + 1;

        size_t null_char = content.find('\0', pos);
        std::string name = content.substr(pos, null_char - pos);
        pos = null_char + 1;

        std::string hash = content.substr(pos, 20); 
        
        std::string hex_hash;
        for (unsigned char c : hash) {
            char buf[3]; sprintf(buf, "%02x", c); hex_hash += buf;
        }
        pos += 20;

        std::string full_path = prefix.empty() ? name : prefix + "/" + name;
        
        if(!check(full_path, tracked_files)) continue;

        if (mode == "40000") {
            if (!validate_tree(hex_hash, full_path, tracked_files)) return false;
        } else {
            if (!fs::exists(full_path)) {
                return false; 
            }

            std::string disk_content = utils::read_file(full_path);
            std::string header = "blob " + std::to_string(disk_content.size()) + '\0';
            std::string disk_hash = utils::sha1(header + disk_content);
            
            if (disk_hash != hex_hash){
                 return false; 
            }
        }
    }
    return true;
}

bool is_work_tree_clean() {
    std::string hc = utils::read_file(".mvc/HEAD");
    if (hc.empty()) return true;
    while (!hc.empty() && isspace(hc.back())) hc.pop_back();

    std::string head_commit_hash;
    if (hc.rfind("ref: ", 0) == 0) {
        std::string rp = ".mvc/" + hc.substr(5);
        if (fs::exists(rp)) {
            head_commit_hash = utils::read_file(rp);
            while (!head_commit_hash.empty() && isspace(head_commit_hash.back())) 
                head_commit_hash.pop_back();
        }
    } else {
        head_commit_hash = hc;
    }

    if (head_commit_hash.empty()) return true; // Empty repo is clean

    std::string head_tree = get_tree_from_commit(head_commit_hash);

    std::vector<fs::path> tracked_files{};

    {
        std::string tracked_path = ".mvc/global_tracked_files.txt";
        std::ifstream in_tracked_files(tracked_path);

        std::string temp;
        while(in_tracked_files >> temp)
        {
            tracked_files.push_back((fs::path)temp);
        }
    }
    
    // Only validate files that exist in the HEAD tree. 
    // Untracked files on disk are ignored.
    return validate_tree(head_tree, "", tracked_files);
}

bool init_repository() {
    const std::string root_dir = ".mvc";

    if(fs::exists(root_dir)) {
        std::cerr << "Respository already exists. \n";
        return false;
    }

    try{
        fs::create_directories(".mvc");
        fs::create_directory(".mvc/objects");
        fs::create_directory(".mvc/refs");
        fs::create_directory(".mvc/refs/heads");

        utils::write_file(".mvc/HEAD", "ref: refs/heads/master\n");

        std::string name, email;
        std::cout << "Initialized empty MVC repository.\n";
        std::cout << "--- Author Configuration ---\n";  
        std::cout << "Enter your Name: ";
        
        if (std::cin.peek() == '\n') std::cin.ignore(); 
        std::getline(std::cin, name);

        std::cout << "Enter your Email: ";
        std::getline(std::cin, email);
        
        if (name.empty()) name = "user";
        if (email.empty()) email = "user@example.com";

        std::string config_data = name + " <" + email + ">";
        utils::write_file(".mvc/config", config_data);
        
        std::cout << "Identity saved as: " << config_data << "\n";
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error initializing repository: " << e.what() << "\n";
        return false;
    }

}