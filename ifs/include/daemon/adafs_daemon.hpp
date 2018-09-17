
#ifndef IFS_ADAFS_DAEMON_HPP
#define IFS_ADAFS_DAEMON_HPP

// std libs
#include <string>
#include <spdlog/spdlog.h>

// adafs config
#include <global/configure.hpp>
#include <global/global_defs.hpp>
// margo
extern "C" {
#include <abt.h>
#include <mercury.h>
#include <margo.h>
}
// adafs
#include <daemon/classes/fs_data.hpp>
#include <daemon/classes/rpc_data.hpp>

#define INVALID_INODE static_cast<ino_t>(0)
#define ADAFS_DATA (static_cast<FsData*>(FsData::getInstance()))
#define RPC_DATA (static_cast<RPCData*>(RPCData::getInstance()))

bool init_environment();
void destroy_enviroment();

bool init_io_tasklet_pool();
bool init_ipc_server();
bool init_rpc_server();

void register_server_rpcs(margo_instance_id mid);

bool register_daemon_proc();

bool deregister_daemon_proc();

std::string get_my_hostname(bool short_hostname);

#endif //IFS_ADAFS_DAEMON_HPP
