#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

bool init_repository();
bool check(fs::path current_path, const std::vector<fs::path>& added_paths);
bool is_work_tree_clean();
bool validate_tree(const std::string& tree_hash, const std::string& prefix);

std::string store_blob(const std::string& data);
std::string write_tree(const std::vector<fs::path>& added_paths);
std::string build(fs::path path, const std::vector<fs::path>& added_paths);


#endif