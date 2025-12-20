#ifndef REPOSITORY_H
#define REPOSITORY_H

#include <string>
bool init_repository();

std::string store_blob(const std::string& data);
std::string write_tree(const std::string& path);

#endif