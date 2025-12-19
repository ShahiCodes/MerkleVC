#include <iostream>
#include <vector>
#include <string>
#include "repository.h"

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
