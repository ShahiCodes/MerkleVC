#ifndef BRANCH_H
#define BRANCH_H

#include <string>
#include <vector>

bool create_branch(const std::string& branch_name);
std::vector<std::string> list_branches();
bool delete_branch(const std::string& branch_name); // <--- Add this line

#endif // BRANCH_H