#include <iostream>
#include <vector>
#include <string>
#include "repository.h"
#include "utils.h"

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
            std::string data = utils::read_file(file_path);

            std::string header = "blob " + std::to_string(data.size()) + '\0';
            std::string store_data = header + data;

            std::string hash = utils::sha1(store_data);

            std::string compressed_data = utils::compress(store_data);

            std::string dir_name = hash.substr(0,2);
            std::string file_name = hash.substr(2);
            std::string object_path = ".mvc/objects/" + dir_name + "/" + file_name;

            utils::write_file(object_path, compressed_data);
            std::cout << hash << "\n";
        }
        catch(const std::exception& e){
            std::cerr << "Error: "<< e.what() << "\n";
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
