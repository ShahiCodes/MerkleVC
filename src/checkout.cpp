#include "checkout.h"
#include "restore.h"
#include "utils.h"
#include <iostream> 
#include <filesystem>
#include <set>

namespace fs = std::filesystem;

void checkout(const std::string& target){
    std::string commit_hash ;
    bool is_branch = false;

    std::string branch_ref_path = ".mvc/refs/heads/" + target;

    if(fs::exists(branch_ref_path)){
        is_branch = true;

        commit_hash = utils::read_file(branch_ref_path);
        if (!commit_hash.empty() && commit_hash.back() == '\n'){
            commit_hash.pop_back();
        } 
    }
    else{
        commit_hash = target;
    }

    if (restore(commit_hash)) {
        
        if (is_branch) {
            std::string ref_content = "ref: refs/heads/" + target;
            utils::write_file(".mvc/HEAD", ref_content);
            std::cout << "Switched to branch '" << target << "'\n";
        } else {
            std::cout << "Note: switching to '" << target << "'.\n";
            std::cout << "You are in 'detached HEAD' state.\n";
        }
    } else {
        // If restore failed, do nothing. HEAD is unchanged.
        std::cerr << "Checkout failed.\n";
    }
}