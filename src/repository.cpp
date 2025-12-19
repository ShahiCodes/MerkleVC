#include "repository.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

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