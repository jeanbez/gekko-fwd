//
// Created by evie on 9/6/17.
//

#ifndef IFS_METADENTRY_HPP
#define IFS_METADENTRY_HPP

#include "../../main.hpp"

int create_node(const std::string& path, const uid_t uid, const gid_t gid, mode_t mode);

int create_metadentry(const std::string& path, mode_t mode);

int db_val_to_stat(const std::string& path, std::string db_val, struct stat& attr);

std::string get_attr(const std::string& path);

int remove_metadentry(const std::string& path);

int remove_node(const std::string& path);

int update_metadentry_size(const std::string& path, size_t size);

#endif //IFS_METADENTRY_HPP
