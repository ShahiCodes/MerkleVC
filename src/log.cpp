#include "log.h"
#include "utils.h"
#include <iostream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

void print_commit(const std::string& hash, const std::string& content) {
    std::cout << "\033[33mcommit " << hash << "\033[0m\n"; // Yellow color for hash

    std::stringstream ss(content);
    std::string line;

    while(std::getline(ss, line) && !line.empty()){
        if(line.rfind("author ", 0) == 0){
            std::cout << line << "\n";
        }
        if(line.rfind("date ", 0) == 0){
            std::cout << line << "\n\n";
        }
    }

    std::cout << "\n commit message:-";

    while(std::getline(ss, line)){
        std::cout << "    " << line << "\n";
    }
    std::cout  << "\n";
}

std::string get_parent_from_content(const std::string& content){
    std::stringstream ss(content);
    std::string line;

    while(std::getline(ss, line) && !line.empty()){
        if(line.rfind("parent ", 0) == 0){
            return line.substr(7); // Return hash after "parent "
        }
    }

    return "";
}

std::string parse_object_body(const std::string& raw_data){
    size_t null_pos = raw_data.find('\0');
    if(null_pos == std::string::npos){
        return "";
    }
    return raw_data.substr(null_pos + 1);
}
 

void log_history(){
    std::string head_content = utils::read_file(".mvc/HEAD");

    if(head_content.empty()){
        std::cerr << "Error: HEAD file is empty or missing.\n";
        return;
    }

    if(head_content.back() == '\n'){
        head_content.pop_back();
    }

    std::string current_hash;

    if(head_content.rfind("ref: ", 0) == 0){
        std::string ref_path = ".mvc/" +head_content.substr(5);

        if(fs::exists(ref_path)){
            current_hash = utils::read_file(ref_path);
            if(!current_hash.empty() && current_hash.back() == '\n'){
                current_hash.pop_back();
            }
        }
        else{
            std::cerr << "Error: Reference path " << ref_path << " does not exist.\nNo commits yet. \n";
            return;
        }
    }
    else{
        current_hash = head_content;
    }

    while(!current_hash.empty()){
        std::string dir = current_hash.substr(0, 2);
        std::string file = current_hash.substr(2);
        std::string path = ".mvc/objects/" + dir + "/" + file;

        if (!fs::exists(path)) {
            std::cerr << "Error: Object " << current_hash << " not found (repository corruption).\n";
            break;
        }

        // Read and Decompress
        std::string compressed = utils::read_file(path);
        std::string raw = utils::decompress(compressed);
        std::string body = parse_object_body(raw);

        print_commit(current_hash, body);

        // Move to parent
        current_hash = get_parent_from_content(body);
    }
}