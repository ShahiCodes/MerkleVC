#include <iostream>
#include <vector>
#include <string>
#include "repository.h"
#include "utils.h"
#include "commit.h"
#include "restore.h"
#include "log.h"
#include "branch.h"
#include "checkout.h"


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

    if(command == "--help" || command == "help" || command == "-h"){
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

    else if (command == "restore") {
        if (args.size() < 2) {
            std::cerr << "Usage: mvc restore <commit_hash>\n";
            return 1;
        }

        if (!is_work_tree_clean()) {
            std::cerr << "Error: Your changes to the local files would be overwritten by restore.\n";
            std::cerr << "Please commit your changes first.\n";
            return 1;
        }
        
        std::string commit_hash = args[1];
        try {
            restore(commit_hash);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    else if (command == "branch") {
        
        if (args.size() == 1) {
            std::vector<std::string> branches = list_branches();
            
            std::string head_content = utils::read_file(".mvc/HEAD");
            while (!head_content.empty() && isspace(head_content.back())) head_content.pop_back();

            std::string current_branch = "";
            if (head_content.rfind("ref: refs/heads/", 0) == 0) {
                current_branch = head_content.substr(16);
            }

            for (const auto& b : branches) {
                if (b == current_branch) {
                    std::cout << "* \033[32m" << b << "\033[0m\n"; 
                } else {
                    std::cout << "  " << b << "\n";
                }
            }
        } 
        else if (args.size() == 2) {
            std::string branch_name = args[1]; 
            create_branch(branch_name);
        }
        else if (args.size() == 3 && args[1] == "-d") {
            std::string branch_name = args[2];
            delete_branch(branch_name);
        }
        else {
            std::cerr << "Usage: \n";
            std::cerr << "  mvc branch              (List branches)\n";
            std::cerr << "  mvc branch <name>       (Create branch)\n";
            std::cerr << "  mvc branch -d <name>    (Delete branch)\n";
            return 1;
        }
    }

    else if(command == "checkout"){
        if (args.size() < 2) {
            std::cerr << "Usage: mvc checkout <branch_name_or_commit_hash>\n";
            return 1;
        }

        // Safety Guard
        if (!is_work_tree_clean()) {
            std::cerr << "Error: Uncommitted changes. Commit or stash them before checkout.\n";
            return 1;
        }
        std::string target = args[1];
        try {
            checkout(target); 
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

void print_help() {
    std::cout << "MerkleVC - A Merkle DAG Version Control System\n";
    std::cout << "Usage: mvc <command> [<args>]\n\n";
    std::cout << "High-Level Commands (Porcelain):\n";
    std::cout << "   init                     Initialize a new repository in the current directory.\n";
    std::cout << "   commit -m \"message\"      Snapshot the directory and save it to history.\n";
    std::cout << "   log                      Display the commit history (hash, author, message).\n";
    std::cout << "   restore <commit_hash>    Restore the working directory to a specific commit.\n";
    std::cout << "   branch                   List all branches.\n";
    std::cout << "   branch <name>            Create a new branch.\n";
    std::cout << "   branch -d <name>         Delete a branch.\n";
    std::cout << "   help/-h/--help           Show this help message.\n\n";

    std::cout << "Low-Level Commands (Plumbing):\n";
    std::cout << "   write-tree               Compute the tree object for the current directory.\n";
    std::cout << "   hash-object <file>       Compute the object ID for a file and store it.\n";
    std::cout << "\n";
}
