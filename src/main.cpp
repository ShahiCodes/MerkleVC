#include <iostream>
#include <vector>
#include <string>
#include "repository.h"
#include "utils.h"
#include "commit.h"
#include "checkout.h"
#include "log.h"

void print_help();

int main(int argc, char* argv[]) {

    std::vector<std::string> args;
    for(int i = 1; i < argc; i++){
        args.push_back(std::string(argv[i]));
    }
    if(args.empty()){
        print_help();
        return 0;
    }

    const std::string command = args[0];

    if(command == "--help"){
        print_help();
        return 0;
    }
    else if (command == "init"){
        if(init_repository()){
            return 0;
        }
        else{
            return 1;
        }
    }
    else if(command == "hash-object"){
        if(args.size() < 2){
            std::cerr << "Usage: mvc hash-object <file_path>\n";
            return 1;
        }
        std::string file_path = args[1];
        try{
            std::string hash = store_blob(file_path);
            std::cout << hash << "\n";
        }
        catch(const std::exception& e){
            std::cerr << "Error: "<< e.what() << "\n";
            return 1;
        }
    }
    else if (command == "write-tree") {
        try {
            // Default to "." if no argument provided
            std::string path = ".";
            if (args.size() > 1) {
                path = args[1];
            }
            
            std::string hash = write_tree(path);
            std::cout << hash << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }
    else if(command == "commit"){
        if (args.size() < 3 || args[1] != "-m"){
            std::cerr << "Usage: mvc commit -m <message>\n";
            return 1;
        }

        std::string message = args[2];

        try{
            std::string tree_hash = write_tree(".");
            std::string commit_hash = commit_tree(tree_hash, message);

            std::cout  << "[" << commit_hash << "] " << message << "\n";
        }
        catch(const std::exception& e){
            std::cerr << "Error: "<< e.what() << "\n";
            return 1;
        }
    }

    else if(command == "log"){
        try{
            log_history();
        } 
        catch(const std::exception& e){
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    else if (command == "checkout") {
        if (args.size() < 2) {
            std::cerr << "Usage: mvc checkout <commit_hash>\n";
            return 1;
        }

        if (!is_work_tree_clean()) {
            std::cerr << "Error: Your changes to the local files would be overwritten by checkout.\n";
            std::cerr << "Please commit your changes first.\n";
            return 1;
        }
        
        std::string commit_hash = args[1];
        try {
            checkout(commit_hash);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    else{
        std::cerr << "unknown command " << command << "\n";
        std::cerr << "run ./mvc --help \n";

        return 1;
    }

    return 0;
}

void print_help(){
    std::cout << "MerkleVC (mvc) - A simple C++ version control system\n\n";
    std::cout << "Usage:\n";
    std::cout << "  mvc <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  init        Initialize a new repository\n";
    std::cout << "  --help      Show this help message\n";
}
