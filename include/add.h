#ifndef ADD_H
#define ADD_H

#include <string>
#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <set>
#include "utils.h"


namespace fs = std::filesystem;

//Validates if the files actually exist or not and returns the files which exists
void add_files(const std::vector<std::string>& files);

#endif