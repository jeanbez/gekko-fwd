#ifndef IFS_PATH_UTIL_HPP
#define IFS_PATH_UTIL_HPP

#include <string>

bool is_relative_path(const std::string& path);
bool is_absolute_path(const std::string& path);
bool has_trailing_slash(const std::string& path);

std::string path_to_relative(const std::string& root_path, const std::string& complete_path);
std::string dirname(const std::string& path);

#endif //IFS_PATH_UTIL_HPP
