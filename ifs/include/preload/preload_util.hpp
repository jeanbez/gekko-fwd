
#ifndef IFS_PRELOAD_UTIL_HPP
#define IFS_PRELOAD_UTIL_HPP

#include <preload/preload.hpp>
// third party libs
#include <string>
#include <iostream>

extern "C" {
#include <margo.h>
}

// Used to bundle metadata into one place
struct Metadentry {
    time_t atime;
    time_t mtime;
    time_t ctime;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    uint64_t inode_no;
    nlink_t link_count;
    off_t size;
    blkcnt_t blocks;

    std::string path;
};
struct MetadentryUpdateFlags {
    bool atime = false;
    bool mtime = false;
    bool ctime = false;
    bool uid = false;
    bool gid = false;
    bool mode = false;
    bool inode_no = false;
    bool link_count = false;
    bool size = false;
    bool blocks = false;
    bool path = false;
};

// Margo instances
extern margo_instance_id ld_margo_ipc_id;
extern margo_instance_id ld_margo_rpc_id;
// local daemon address for margo ipc client
extern hg_addr_t daemon_svr_addr;
// IPC IDs
extern hg_id_t ipc_minimal_id;
extern hg_id_t ipc_config_id;
extern hg_id_t ipc_mk_node_id;
extern hg_id_t ipc_access_id;
extern hg_id_t ipc_stat_id;
extern hg_id_t ipc_rm_node_id;
extern hg_id_t ipc_decr_size_id;
extern hg_id_t ipc_update_metadentry_id;
extern hg_id_t ipc_get_metadentry_size_id;
extern hg_id_t ipc_update_metadentry_size_id;
extern hg_id_t ipc_write_data_id;
extern hg_id_t ipc_read_data_id;
extern hg_id_t ipc_trunc_data_id;
extern hg_id_t ipc_get_dirents_id;
// RPC IDs
extern hg_id_t rpc_minimal_id;
extern hg_id_t rpc_mk_node_id;
extern hg_id_t rpc_stat_id;
extern hg_id_t rpc_access_id;
extern hg_id_t rpc_rm_node_id;
extern hg_id_t rpc_decr_size_id;
extern hg_id_t rpc_update_metadentry_id;
extern hg_id_t rpc_get_metadentry_size_id;
extern hg_id_t rpc_update_metadentry_size_id;
extern hg_id_t rpc_write_data_id;
extern hg_id_t rpc_read_data_id;
extern hg_id_t rpc_trunc_data_id;
extern hg_id_t rpc_get_dirents_id;
// rpc addresses. Populated when environment is initialized. After that it is read-only accessed
extern std::map<uint64_t, hg_addr_t> rpc_addresses;
// file descriptor index validation flag
extern std::atomic<bool> fd_validation_needed;

// function definitions

int generate_fd_idx();

int get_fd_idx();

bool is_fs_path(const char* path);

// TODO template these two suckers
int db_val_to_stat(std::string path, std::string db_val, struct stat& attr);

int db_val_to_stat64(std::string path, std::string db_val, struct stat64& attr);

int get_daemon_pid();

bool read_system_hostfile();

bool lookup_all_hosts();

bool get_addr_by_hostid(uint64_t hostid, hg_addr_t& svr_addr);

bool is_local_op(size_t recipient);

hg_return margo_create_wrap(hg_id_t ipc_id, hg_id_t rpc_id, const std::string&, hg_handle_t& handle, bool force_rpc);

hg_return margo_create_wrap(const hg_id_t ipc_id, const hg_id_t rpc_id, const size_t& recipient, hg_handle_t& handle, bool force_rpc);

#endif //IFS_PRELOAD_UTIL_HPP
