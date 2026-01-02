#ifndef COMMIT_H
#define COMMIT_H

#include <string>
#include <vector>

std::string commit_tree(const std::string& tree_sha, const std::string& message, const std::vector<std::string>&parent_hashes = {});

#endif