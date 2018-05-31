#include <global/path_util.hpp>
#include <cassert>


const constexpr char PSP('/'); // PATH SEPARATOR

bool is_relative_path(const std::string& path) {
    return (!path.empty()) &&
           (path.front() != PSP);
}

bool is_absolute_path(const std::string& path) {
    return (!path.empty()) &&
           (path.front() == PSP);
}

bool has_trailing_slash(const std::string& path) {
    return path.back() == PSP;
}

/* Make an absolute path relative to a root path
 *
 * Convert @absolute_path into a relative one with respect to the given @root_path.
 * If @absolute_path do not start at the given @root_path an empty string will be returned.
 * NOTE: Trailing slash will be stripped from the new constructed relative path.
 */
std::string path_to_relative(const std::string& root_path, const std::string& absolute_path) {
    assert(is_absolute_path(root_path));
    assert(is_absolute_path(absolute_path));
    assert(!has_trailing_slash(root_path));

    auto diff_its = std::mismatch(absolute_path.cbegin(), absolute_path.cend(), root_path.cbegin());
    if(diff_its.second != root_path.cend()){
        // complete path doesn't start with root_path
        return {};
    }

    // iterator to the starting char of the relative portion of the @absolute_path
    auto rel_it_begin = diff_its.first;
    // iterator to the end of the relative portion of the @absolute_path
    auto rel_it_end = absolute_path.cend();

    // relative path start exactly after the root_path prefix
    assert((size_t)(rel_it_begin - absolute_path.cbegin()) == root_path.size());

    if(rel_it_begin == rel_it_end) {
        //relative path is empty, @absolute_path was equal to @root_path
        return {'/'};
    }

    // remove the trailing slash from relative path
    if(has_trailing_slash(absolute_path) &&
      rel_it_begin != rel_it_end - 1) { // the relative path is longer then 1 char ('/')
        --rel_it_end;
    }

    return {rel_it_begin, rel_it_end};
}

std::string dirname(const std::string& path) {
    assert(path.size() > 1 || path.front() == PSP);
    assert(path.size() == 1 || !has_trailing_slash(path));

    auto parent_path_size = path.find_last_of(PSP);
    assert(parent_path_size != std::string::npos);
    if(parent_path_size == 0) {
        // parent is '/'
        parent_path_size = 1;
    }
    return path.substr(0, parent_path_size);
}
