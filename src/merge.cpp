#include "merge.h"
#include "utils.h"
#include "commit.h" 
#include "restore.h" 
#include "status.h"
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <queue>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <deque>
#include <unordered_map>

namespace fs = std::filesystem;

std::string to_hex(const std::string& bytes) {
    std::stringstream ss;
    for (unsigned char c : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return ss.str();
}

std::string clean_hash(std::string h) {
    while (!h.empty() && (isspace(h.back()) || h.back() == '\0')) h.pop_back();
    while (!h.empty() && (isspace(h.front()) || h.front() == '\0')) h.erase(0, 1);
    return h;
}
std::vector<std::string> get_parent_hashes(const std::string& commit_hash) {
    std::vector<std::string> parents;
    std::string h = clean_hash(commit_hash);
    if (h.empty()) return parents;
    std::string dir = h.substr(0, 2);
    std::string file = h.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    if (!fs::exists(path)) return parents;
    std::string content = utils::decompress(utils::read_file(path));
    size_t pos = 0;
    while ((pos = content.find("\nparent ", pos)) != std::string::npos) {
        pos += 8;
        size_t end = content.find('\n', pos);
        std::string p = clean_hash(content.substr(pos, end - pos));
        if (!p.empty()) parents.push_back(p);
        pos = end;
    }
    return parents;
}

std::string get_blob_content(const std::string& hash) {
    if (hash.empty()) return "";
    std::string dir = hash.substr(0, 2);
    std::string file = hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    if (!fs::exists(path)) return "";
    std::string content = utils::decompress(utils::read_file(path));
    size_t null_pos = content.find('\0');
    if (null_pos != std::string::npos) {
        content = content.substr(null_pos + 1);
    }
    return content;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

struct DiffHunk {
    bool is_equal;
    int old_start;
    int old_end;
    int new_start;
    int new_end;
};

std::vector<DiffHunk> compute_diff(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    int n = (int)a.size(), m = (int)b.size();
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = n - 1; i >= 0; --i) {
        for (int j = m - 1; j >= 0; --j) {
            if (a[i] == b[j]) dp[i][j] = dp[i + 1][j + 1] + 1;
            else dp[i][j] = std::max(dp[i + 1][j], dp[i][j + 1]);
        }
    }
    std::vector<DiffHunk> hunks;
    int i = 0, j = 0;
    while (i < n && j < m) {
        if (a[i] == b[j]) { ++i; ++j; continue; }
        int os = i, ns = j;
        while (true) {
            if (i < n && j < m && a[i] == b[j]) break;
            if (i >= n && j >= m) break;
            if (i < n && j < m) {
                if (dp[i + 1][j] >= dp[i][j + 1]) ++i;
                else ++j;
            } else if (i < n) ++i;
            else ++j;
        }
        DiffHunk h;
        h.is_equal = false;
        h.old_start = os; h.old_end = i;
        h.new_start = ns; h.new_end = j;
        hunks.push_back(h);
    }
    if (i < n || j < m) {
        DiffHunk h;
        h.is_equal = false;
        h.old_start = i; h.old_end = n;
        h.new_start = j; h.new_end = m;
        hunks.push_back(h);
    }
    return hunks;
}

std::string merge_lines(const std::vector<std::string>& base,
                        const std::vector<std::string>& head,
                        const std::vector<std::string>& target,
                        bool& conflict) {
    conflict = false;
    std::vector<DiffHunk> dh = compute_diff(base, head);
    std::vector<DiffHunk> dt = compute_diff(base, target);
    std::vector<std::string> out;
    size_t hi = 0, ti = 0;
    int bp = 0;
    int bsize = (int)base.size();

    while (hi < dh.size() || ti < dt.size() || bp < bsize) {
        int next = bsize;
        const DiffHunk* hh = hi < dh.size() ? &dh[hi] : nullptr;
        const DiffHunk* ht = ti < dt.size() ? &dt[ti] : nullptr;
        if (hh) next = std::min(next, hh->old_start);
        if (ht) next = std::min(next, ht->old_start);
        while (bp < next) out.push_back(base[bp++]);

        bool hc = hh && hh->old_start == bp;
        bool tc = ht && ht->old_start == bp;
        if (hc && tc) {
            std::vector<std::string> hl(head.begin() + hh->new_start, head.begin() + hh->new_end);
            std::vector<std::string> tl(target.begin() + ht->new_start, target.begin() + ht->new_end);
            if (hl == tl) {
                for (auto& l : hl) out.push_back(l);
            } else {
                conflict = true;
                out.push_back("<<<<<<< HEAD");
                for (auto& l : hl) out.push_back(l);
                out.push_back("=======");
                for (auto& l : tl) out.push_back(l);
                out.push_back(">>>>>>> target");
            }
            bp = std::max(hh->old_end, ht->old_end);
            hi++; ti++;
        } else if (hc) {
            for (int k = hh->new_start; k < hh->new_end; ++k) out.push_back(head[k]);
            bp = hh->old_end;
            hi++;
        } else if (tc) {
            for (int k = ht->new_start; k < ht->new_end; ++k) out.push_back(target[k]);
            bp = ht->old_end;
            ti++;
        } else {
            if (bp < bsize) out.push_back(base[bp++]);
            else break;
        }
    }

    std::ostringstream os;
    for (size_t i = 0; i < out.size(); ++i) {
        os << out[i];
        if (i + 1 < out.size() || !out.back().empty()) os << '\n';
    }
    return os.str();
}

std::string find_lca(const std::string& commit1, const std::string& commit2) {
    std::string c1 = clean_hash(commit1);
    std::string c2 = clean_hash(commit2);
    if (c1 == c2) return c1;
    std::unordered_map<std::string, int> visited1, visited2;
    std::deque<std::string> queue1, queue2;
    queue1.push_back(c1);
    visited1[c1] = 0;
    queue2.push_back(c2);
    visited2[c2] = 0;
    
    int safety = 0;
    while (!queue1.empty() && !queue2.empty() && safety < 10000) {
        if (!queue1.empty()) {
            std::string current = queue1.front();
            queue1.pop_front();
            int depth = visited1[current];
            if (visited2.find(current) != visited2.end()) {
                return current;
            }
            for (const auto& p : get_parent_hashes(current)) {
                if (visited1.find(p) == visited1.end()) {
                    visited1[p] = depth + 1;
                    queue1.push_back(p);
                }
            }
        }
        if (!queue2.empty()) {
            std::string current = queue2.front();
            queue2.pop_front();
            int depth = visited2[current];
            if (visited1.find(current) != visited1.end()) {
                return current;
            }
            for (const auto& p : get_parent_hashes(current)) {
                if (visited2.find(p) == visited2.end()) {
                    visited2[p] = depth + 1;
                    queue2.push_back(p);
                }
            }
        }
        safety++;
    }
    return "";
}

void collect_files(const std::string& raw_tree_hash, const std::string& prefix, std::map<std::string, std::string>& file_map) {
    std::string tree_hash = clean_hash(raw_tree_hash);
    if (tree_hash.length() < 4) return;

    std::string dir = tree_hash.substr(0, 2);
    std::string file = tree_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return;

    std::string content = utils::decompress(utils::read_file(path));
    
    size_t pos = content.find('\0');
    if (pos == std::string::npos) return; 
    pos++; 

    while (pos < content.size()) {
        size_t space_pos = content.find(' ', pos);
        if (space_pos == std::string::npos) break;
        std::string mode = content.substr(pos, space_pos - pos);
        pos = space_pos + 1;

        size_t null_pos = content.find('\0', pos);
        if (null_pos == std::string::npos) break;
        std::string name = content.substr(pos, null_pos - pos);
        pos = null_pos + 1;

        if (pos + 20 > content.size()) break;
        std::string raw_hash = content.substr(pos, 20);
        std::string hex_hash = to_hex(raw_hash);
        pos += 20;

        if (mode == "40000") { 
            collect_files(hex_hash, prefix + name + "/", file_map);
        } else { 
            file_map[prefix + name] = hex_hash;
        }
    }
}

std::map<std::string, std::string> get_file_map(const std::string& raw_commit_hash) {
    std::map<std::string, std::string> files;
    std::string commit_hash = clean_hash(raw_commit_hash);
    if (commit_hash.empty()) return files;

    std::string dir = commit_hash.substr(0, 2);
    std::string file = commit_hash.substr(2);
    std::string path = ".mvc/objects/" + dir + "/" + file;
    
    if (!fs::exists(path)) return files;

    std::string content = utils::decompress(utils::read_file(path));
    
    size_t tree_pos = content.find("tree ");
    if (tree_pos != std::string::npos) {
        size_t end_line = content.find('\n', tree_pos);
        std::string tree_hash = content.substr(tree_pos + 5, end_line - (tree_pos + 5));
        collect_files(tree_hash, "", files);
    }
    return files;
}

bool merge_branch(const std::string& branch_name) {
    if (has_uncommitted_changes()) {
        std::cerr << "Error: Cannot merge with uncommitted changes. Commit or discard changes first.\n";
        return false;
    }

    if (fs::exists(".mvc/MERGE_HEAD")) {
        std::cerr << "Error: A merge is already in progress (MERGE_HEAD exists).\n";
        std::cerr << "Complete the merge with 'mvc commit' or abort.\n";
        return false;
    }

    std::string target_hash, head_hash;

    std::string ref_path = ".mvc/refs/heads/" + branch_name;
    if (fs::exists(ref_path)) target_hash = clean_hash(utils::read_file(ref_path));
    else { std::cerr << "Error: Branch '" << branch_name << "' not found.\n"; return false; }

    std::string head_content = utils::read_file(".mvc/HEAD");
    std::string current_branch_ref = "";
    while(!head_content.empty() && isspace(head_content.back())) head_content.pop_back();

    if (head_content.rfind("ref: ", 0) == 0) {
        current_branch_ref = head_content;

        std::string h_path = ".mvc/" + head_content.substr(5);
        if (fs::exists(h_path)) head_hash = clean_hash(utils::read_file(h_path));
    } else {
        head_hash = clean_hash(head_content);
    }

    if (head_hash == target_hash) { std::cout << "Already up to date.\n"; return true; }

    std::string ancestor_hash = find_lca(head_hash, target_hash);
    
    if (ancestor_hash == head_hash) {
        std::cout << "Fast-forward merge...\n";
        if (!restore(target_hash)) {
            std::cerr << "Fast-forward merge failed.\n";
            return false;
        }
        if (!current_branch_ref.empty()) {
            std::string branch_file = ".mvc/" + current_branch_ref.substr(5);
            utils::write_file(branch_file, target_hash);
            utils::write_file(".mvc/HEAD", current_branch_ref);
        }
        fs::remove(".mvc/MERGE_HEAD");
        fs::remove(".mvc/MERGE_CONFLICTS");
        return true;
    }

    std::cout << "Merging: Base=" << ancestor_hash.substr(0,7) 
              << " Head=" << head_hash.substr(0,7) 
              << " Target=" << target_hash.substr(0,7) << "\n";

    auto base_files = get_file_map(ancestor_hash);
    auto head_files = get_file_map(head_hash);
    auto target_files = get_file_map(target_hash);

    std::set<std::string> all_files;
    for (const auto& [f, h] : base_files) all_files.insert(f);
    for (const auto& [f, h] : head_files) all_files.insert(f);
    for (const auto& [f, h] : target_files) all_files.insert(f);

    bool has_conflict = false;
    std::vector<std::string> conflicted_files;

    for (const auto& file : all_files) {
        std::string h_base = base_files.count(file) ? base_files[file] : "";
        std::string h_head = head_files.count(file) ? head_files[file] : "";
        std::string h_target = target_files.count(file) ? target_files[file] : "";

        if (h_head == h_target) continue;

        if (h_head == h_base) {
            if (h_target.empty()) {
                std::cout << "Deleting: " << file << "\n";
                fs::remove(file);
            } else {
                std::cout << "Updating: " << file << "\n";
                std::string content = get_blob_content(h_target);
                if (fs::path(file).has_parent_path()) fs::create_directories(fs::path(file).parent_path());
                utils::write_file(file, content);
            }
            continue;
        }
        if (h_target == h_base) {
            if (h_head.empty()) {
                std::cout << "Deleting: " << file << "\n";
                fs::remove(file);
            } else {
                std::cout << "Updating: " << file << "\n";
                std::string content = get_blob_content(h_head);
                if (fs::path(file).has_parent_path()) fs::create_directories(fs::path(file).parent_path());
                utils::write_file(file, content);
            }
            continue;
        }

        if (h_head.empty() || h_target.empty()) {
            std::cerr << "CONFLICT (delete/modify): " << file << "\n";
            has_conflict = true;
            conflicted_files.push_back(file);
            std::string head_content = h_head.empty() ? "" : get_blob_content(h_head);
            std::string target_content = h_target.empty() ? "" : get_blob_content(h_target);
            std::string merged = "<<<<<<< HEAD\n" + head_content + "\n=======\n" + target_content + "\n>>>>>>> target\n";
            utils::write_file(file, merged);
            continue;
        }

        std::string base_content = get_blob_content(h_base);
        std::string head_content_blob = get_blob_content(h_head);
        std::string target_content_blob = get_blob_content(h_target);

        bool conflict = false;
        std::string merged_content = merge_lines(
            split_lines(base_content),
            split_lines(head_content_blob),
            split_lines(target_content_blob),
            conflict
        );
        if (conflict) {
            std::cerr << "CONFLICT (content): " << file << "\n";
            has_conflict = true;
            conflicted_files.push_back(file);
        } else {
            std::cout << "Merged: " << file << "\n";
        }
        if (fs::path(file).has_parent_path()) fs::create_directories(fs::path(file).parent_path());
        utils::write_file(file, merged_content);
    }

    utils::write_file(".mvc/MERGE_HEAD", target_hash);
    if (has_conflict) {
        std::ofstream conf_file(".mvc/MERGE_CONFLICTS");
        for (const auto& f : conflicted_files) {
            conf_file << f << "\n";
        }
        conf_file.close();
        std::cerr << "Merge completed with conflicts.\n";
        std::cerr << "Resolve conflicts and then commit.\n";
    } else {
        fs::remove(".mvc/MERGE_CONFLICTS");
        std::cout << "Merge successful. updating index...\n";
        std::cout << "ACTION REQUIRED: Files merged.\n";
        std::cout << "Run: ./mvc commit -m \"Merge branch " << branch_name << "\"\n";
    }
    return true;
}