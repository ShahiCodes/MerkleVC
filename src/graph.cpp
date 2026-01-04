#include "graph.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

std::map<std::string, std::string> get_labels() {
    std::map<std::string, std::string> labels;
    
    std::string head_content = utils::read_file(".mvc/HEAD");
    while (!head_content.empty() && isspace(head_content.back())) head_content.pop_back();
    std::string active_branch = "";
    bool detached = true;

    if (head_content.rfind("ref: refs/heads/", 0) == 0) {
        active_branch = head_content.substr(16);
        detached = false;
    }

    if (fs::exists(".mvc/refs/heads")) {
        for (const auto& entry : fs::directory_iterator(".mvc/refs/heads")) {
            std::string name = entry.path().filename().string();
            std::string hash = utils::read_file(entry.path().string());
            while (!hash.empty() && isspace(hash.back())) hash.pop_back();

            std::string tag = "\033[1;32m" + name + "\033[0m"; 
            if (!detached && name == active_branch) {
                tag += " \033[1;36m-> HEAD\033[0m"; 
            }
            if (labels.count(hash)) labels[hash] += ", " + tag;
            else labels[hash] = " (" + tag + ")";
        }
    }
    
    if (detached && !head_content.empty()) {
        if (labels.count(head_content)) labels[head_content] += ", \033[1;36mHEAD\033[0m";
        else labels[head_content] = " (\033[1;36mHEAD\033[0m)";
    }
    return labels;
}

struct CommitInfo {
    std::vector<std::string> parents;
    std::string message;
};

CommitInfo get_commit_info(const std::string& hash) {
    CommitInfo info;
    std::string dir = hash.substr(0, 2);
    std::string file = hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;

    if (!fs::exists(path)) return info;

    std::string content = utils::decompress(utils::read_file(path));
    
    std::stringstream ss(content);
    std::string line;
    bool in_message = false;
    
    while (std::getline(ss, line)) {
        if (line.empty()) {
            in_message = true;
            continue;
        }
        
        if (!in_message) {
            if (line.rfind("parent ", 0) == 0) {
                std::string p = line.substr(7);
                while (!p.empty() && isspace(p.back())) p.pop_back();
                info.parents.push_back(p);
            }
        } else {
            if (info.message.empty()) {
                info.message = line;
                if (info.message.length() > 60) info.message = info.message.substr(0, 57) + "...";
            }
        }
    }
    return info;
}

void print_subtree(const std::string& current_hash, 
                   const std::map<std::string, std::vector<std::string>>& children_map,
                   const std::map<std::string, std::string>& labels,
                   const std::map<std::string, std::string>& messages,
                   const std::string& prefix, 
                   bool is_last,
                   std::set<std::string>& drawn) { 
    
    std::string marker = is_last ? "└── " : "├── ";
    
    bool already_drawn = drawn.count(current_hash);

    std::cout << prefix << marker << "\033[33m" << current_hash.substr(0, 7) << "\033[0m"; 
    
    if (already_drawn) {
        std::cout << " \033[90m(see above)\033[0m\n";
        return;
    }

    drawn.insert(current_hash);

    if (labels.count(current_hash)) std::cout << labels.at(current_hash);
    
    if (messages.count(current_hash)) std::cout << " \033[90m" << messages.at(current_hash) << "\033[0m";
    std::cout << "\n";

    std::string child_prefix = prefix + (is_last ? "    " : "│   ");

    if (children_map.count(current_hash)) {
        const auto& children = children_map.at(current_hash);
        for (size_t i = 0; i < children.size(); ++i) {
            print_subtree(children[i], children_map, labels, messages, child_prefix, i == children.size() - 1, drawn);
        }
    }
}

void show_graph() {
    std::map<std::string, std::vector<std::string>> children_map;
    std::map<std::string, std::string> messages;
    std::set<std::string> visited;
    std::vector<std::string> queue;
    
    if (fs::exists(".mvc/refs/heads")) {
        for (const auto& entry : fs::directory_iterator(".mvc/refs/heads")) {
            std::string h = utils::read_file(entry.path().string());
            while (!h.empty() && isspace(h.back())) h.pop_back();
            if (!h.empty()) queue.push_back(h);
        }
    }
    std::string head = utils::read_file(".mvc/HEAD");
    if (head.find("ref:") == std::string::npos && !head.empty()) {
        while (!head.empty() && isspace(head.back())) head.pop_back();
        queue.push_back(head);
    }

    std::set<std::string> roots;

    while (!queue.empty()) {
        std::string current = queue.back();
        queue.pop_back();

        if (visited.count(current)) continue;
        visited.insert(current);

        CommitInfo info = get_commit_info(current);
        messages[current] = info.message;

        if (info.parents.empty()) {
            roots.insert(current);
        } else {
            for (const auto& p : info.parents) {
                children_map[p].push_back(current);
                if (!visited.count(p)) queue.push_back(p);
            }
        }
    }

    if (roots.empty()) {
        std::cout << "No history found.\n";
        return;
    }

    std::map<std::string, std::string> labels = get_labels();
    std::set<std::string> drawn; 

    std::cout << "Repository Graph:\n";
    for (const auto& root : roots) {
        print_subtree(root, children_map, labels, messages, "", true, drawn);
        std::cout << "\n";
    }
}