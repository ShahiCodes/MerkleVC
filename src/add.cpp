#include "add.h"
#include <set>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

void add_files(const std::vector<std::string> &files)
{   
    std::string staging_path = ".mvc/global_valid_files.txt";
    std::string tracking_path = ".mvc/global_tracked_files.txt";

    std::set<std::string> tracked_files;
    std::set<std::string> staging_files;
    {
        std::ifstream in(tracking_path);
        std::ifstream in_stage(staging_path);
        if(in.is_open()) {
            std::string temp;
            while(in >> temp) {
                tracked_files.insert(temp);
            }
        }

        if(in_stage.is_open())
        {
            std::string temp;
            while(in_stage >> temp){
                staging_files.insert(temp);
            }
        }
    } 
    
    std::ofstream staging_out(staging_path, std::ios::app);

    auto process_file = [&](const std::string& raw_path) {
        std::string p = raw_path;
        if(p.size() > 2 && p.substr(0, 2) == "./") p = p.substr(2);

        staging_files.insert(p);
        
        tracked_files.insert(p);
    };

    if (files.size() == 1 && files[0] == ".")
    {
        for (const auto &entry : fs::recursive_directory_iterator(files[0]))
        {
            std::string path_str = entry.path().string();
            std::string name = entry.path().filename().string();

            if (utils::is_ignored(name)) {
                // std::cout << "Ignored " << path_str << '\n';
                continue;
            }

            if(!fs::is_directory(entry.path())) {
                // std::cout << "Adding: " << path_str << '\n';
                process_file(path_str);
            }
        }
    }
    else
    {
        for (const auto &v : files)
        {
            if (fs::exists(v)) {
                if(fs::is_directory(v)) {
                    for(const auto& entry : fs::recursive_directory_iterator(v)) {
                        if(!fs::is_directory(entry.path())) {
                            // std::cout << "Adding: " << entry.path().string() << '\n';
                            process_file(entry.path().string());
                        }
                    }
                } else {
                    // std::cout << "Adding: " << v << '\n';
                    process_file(v);
                }
            }
            else {
                std::cout << "Error: " << v << " does not exist\n";
            }
        }
    }    
    {
        std::ofstream tracking_out(tracking_path, std::ios::trunc);
        for(const auto& f : tracked_files)
        {
            tracking_out << f << "\n";
        }

        std::ofstream staging_out(staging_path, std::ios::trunc);
        for(const auto& f : staging_files) staging_out << f << '\n';
    }
}