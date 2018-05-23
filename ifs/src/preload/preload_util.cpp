
#include <preload/preload_util.hpp>
#include <global/rpc/rpc_utils.hpp>
#include <global/rpc/distributor.hpp>
#include <global/global_func.hpp>

#include <fstream>
#include <iterator>
#include <sstream>
#include <csignal>
#include <random>

using namespace std;

static const std::string dentry_val_delim = ","s;

/*
 * TODO: Setting our file descriptor index to a specific value is dangerous because we might clash with the kernel.
 * E.g., if we would passthrough and not intercept and the kernel assigns a file descriptor but we will later use
 * the same fd value, we will intercept calls that were supposed to be going to the kernel. This works the other way around too.
 * To mitigate this issue, we set the initial fd number to a high value. We "hope" that we do not clash but this is no permanent solution.
 * Note: This solution will probably work well already for many cases because kernel fd values are reused, unlike to ours.
 * The only case where we will clash with the kernel is, if one process has more than 100000 files open at the same time.
 */
static int fd_idx = 100000;
static mutex fd_idx_mutex;
std::atomic<bool> fd_validation_needed(false);

/**
 * Generate new file descriptor index to be used as an fd within one process in ADA-FS
 * @return fd_idx
 */
int generate_fd_idx() {
    // We need a mutex here for thread safety
    std::lock_guard<std::mutex> inode_lock(fd_idx_mutex);
    if (fd_idx == std::numeric_limits<int>::max()) {
        CTX->log()->info("{}() File descriptor index exceeded ints max value. Setting it back to 100000", __func__);
        /*
         * Setting fd_idx back to 3 could have the effect that fd are given twice for different path.
         * This must not happen. Instead a flag is set which tells can tell the OpenFileMap that it should check
         * if this fd is really safe to use.
         */
        fd_idx = 100000;
        fd_validation_needed = true;
    }
    return fd_idx++;
}

int get_fd_idx() {
    std::lock_guard<std::mutex> inode_lock(fd_idx_mutex);
    return fd_idx;
}

bool is_fs_path(const char* path) {
    return strstr(path, CTX->mountdir().c_str()) == path;
}

// TODO merge the two stat functions
/**
 * Converts the dentry db value into a stat struct, which is needed by Linux
 * @param path
 * @param db_val
 * @param attr
 * @return
 */
int db_val_to_stat(const std::string path, std::string db_val, struct stat& attr) {

    attr.st_ino = std::hash<std::string>{}(path);
    
    auto pos = db_val.find(dentry_val_delim);
    if (pos == std::string::npos) { // no delimiter found => no metadata enabled. fill with dummy values
        attr.st_mode = static_cast<unsigned int>(stoul(db_val));
        attr.st_nlink = 1;
        attr.st_uid = CTX->fs_conf()->uid;
        attr.st_gid = CTX->fs_conf()->gid;
        attr.st_size = 0;
        attr.st_blksize = BLOCKSIZE;
        attr.st_blocks = 0;
        attr.st_atim.tv_sec = 0;
        attr.st_mtim.tv_sec = 0;
        attr.st_ctim.tv_sec = 0;
        return 0;
    }
    // some metadata is enabled: mode is always there
    attr.st_mode = static_cast<unsigned int>(stoul(db_val.substr(0, pos)));
    db_val.erase(0, pos + 1);
    // size is also there XXX
    pos = db_val.find(dentry_val_delim);
    if (pos != std::string::npos) {  // delimiter found. more metadata is coming
        attr.st_size = stol(db_val.substr(0, pos));
        db_val.erase(0, pos + 1);
    } else {
        attr.st_size = stol(db_val);
    }
    // The order is important. don't change.
    if (CTX->fs_conf()->atime_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_atim.tv_sec = static_cast<time_t>(stol(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->mtime_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_mtim.tv_sec = static_cast<time_t>(stol(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->ctime_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_ctim.tv_sec = static_cast<time_t>(stol(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->uid_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_uid = static_cast<uid_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    } else {
        attr.st_uid = CTX->fs_conf()->uid;
    }

    if (CTX->fs_conf()->gid_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_gid = static_cast<uid_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    } else {
        attr.st_gid = CTX->fs_conf()->gid;
    }

    if (CTX->fs_conf()->inode_no_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_ino = static_cast<ino_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->link_cnt_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_nlink = static_cast<nlink_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->blocks_state) { // last one will not encounter a delimiter anymore
        attr.st_blocks = static_cast<blkcnt_t>(stoul(db_val));
    }
    return 0;
}

/**
 * Converts the dentry db value into a stat64 struct, which is needed by Linux
 * @param path
 * @param db_val
 * @param attr
 * @return
 */
int db_val_to_stat64(const std::string path, std::string db_val, struct stat64& attr) {

    auto pos = db_val.find(dentry_val_delim);
    if (pos == std::string::npos) { // no delimiter found => no metadata enabled. fill with dummy values
        attr.st_ino = std::hash<std::string>{}(path);
        attr.st_mode = static_cast<unsigned int>(stoul(db_val));
        attr.st_nlink = 1;
        attr.st_uid = CTX->fs_conf()->uid;
        attr.st_gid = CTX->fs_conf()->gid;
        attr.st_size = 0;
        attr.st_blksize = BLOCKSIZE;
        attr.st_blocks = 0;
        attr.st_atim.tv_sec = 0;
        attr.st_mtim.tv_sec = 0;
        attr.st_ctim.tv_sec = 0;
        return 0;
    }
    // some metadata is enabled: mode is always there
    attr.st_mode = static_cast<unsigned int>(stoul(db_val.substr(0, pos)));
    db_val.erase(0, pos + 1);
    // size is also there XXX
    pos = db_val.find(dentry_val_delim);
    if (pos != std::string::npos) {  // delimiter found. more metadata is coming
        attr.st_size = stol(db_val.substr(0, pos));
        db_val.erase(0, pos + 1);
    } else {
        attr.st_size = stol(db_val);
    }
    // The order is important. don't change.
    if (CTX->fs_conf()->atime_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_atim.tv_sec = static_cast<time_t>(stol(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->mtime_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_mtim.tv_sec = static_cast<time_t>(stol(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->ctime_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_ctim.tv_sec = static_cast<time_t>(stol(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->uid_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_uid = static_cast<uid_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    } else {
        attr.st_uid = CTX->fs_conf()->gid;
    }
    if (CTX->fs_conf()->gid_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_gid = static_cast<uid_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    } else {
        attr.st_gid = CTX->fs_conf()->gid;
    }
    if (CTX->fs_conf()->inode_no_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_ino = static_cast<ino_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->link_cnt_state) {
        pos = db_val.find(dentry_val_delim);
        attr.st_nlink = static_cast<nlink_t>(stoul(db_val.substr(0, pos)));
        db_val.erase(0, pos + 1);
    }
    if (CTX->fs_conf()->blocks_state) { // last one will not encounter a delimiter anymore
        attr.st_blocks = static_cast<blkcnt_t>(stoul(db_val));
    }
    return 0;
}

/**
 * @return daemon pid. If not running @return -1.
 * Loads set deamon mountdir set in daemon.pid file
 */
int get_daemon_pid() {
    ifstream ifs(daemon_pid_path(), ::ifstream::in);
    int adafs_daemon_pid = -1;
    string mountdir;
    if (ifs) {
        string adafs_daemon_pid_s;
        // first line is pid
        if (getline(ifs, adafs_daemon_pid_s) && !adafs_daemon_pid_s.empty())
            adafs_daemon_pid = ::stoi(adafs_daemon_pid_s);
        else {
            cerr << "ADA-FS daemon pid not found. Daemon not running?" << endl;
            CTX->log()->error("{}() Unable to read daemon pid from pid file", __func__);
            ifs.close();
            return -1;
        }
        // check that daemon is running
        if (kill(adafs_daemon_pid, 0) != 0) {
            cerr << "ADA-FS daemon process with pid " << adafs_daemon_pid << " not found. Daemon not running?" << endl;
            CTX->log()->error("{}() ADA-FS daemon pid {} not found. Daemon not running?", __func__, adafs_daemon_pid);
            ifs.close();
            return -1;
        }
        // second line is mountdir
        if (getline(ifs, mountdir) && !mountdir.empty()) {
            CTX->mountdir(mountdir);
        } else {
            CTX->log()->error("{}() ADA-FS daemon pid file contains no mountdir path. Exiting ...", __func__);
            ifs.close();
            return -1;
        }
    } else {
        cerr << "No permission to open pid file at " << daemon_pid_path()
             << " or ADA-FS daemon pid file not found. Daemon not running?" << endl;
        CTX->log()->error(
                "{}() No permission to open pid file at {} or ADA-FS daemon pid file not found. Daemon not running?",
                __func__, daemon_pid_path());
    }
    ifs.close();

    return adafs_daemon_pid;
}

/**
 * Read /etc/hosts and put hostname - ip association into a map in fs config.
 * We are working with hostnames but some network layers (such as Omnipath) does not look into /etc/hosts.
 * Hence, we have to store the mapping ourselves.
 * @return success
 */
bool read_system_hostfile() {
    ifstream hostfile("/etc/hosts");
    if (!hostfile.is_open())
        return false;
    string line;
    map<string, string> sys_hostfile;
    while (getline(hostfile, line)) {
        if (line.empty() || line == "\n" || line.at(0) == '#')
            continue;
        std::istringstream iss(line);
        std::vector<string> tmp_list((istream_iterator<string>(iss)), istream_iterator<string>());
        for (unsigned int i = 1; i < tmp_list.size(); i++) {
            if (tmp_list[i].find(HOSTNAME_SUFFIX) != string::npos)
                sys_hostfile.insert(make_pair(tmp_list[i], tmp_list[0]));
        }
    }
    CTX->fs_conf()->sys_hostfile = sys_hostfile;
    CTX->log()->info("{}() /etc/hosts successfully mapped into ADA-FS", __func__);
    return true;
}

bool lookup_all_hosts() {
    vector<uint64_t> hosts(CTX->fs_conf()->host_size);
    // populate vector with [0, ..., host_size - 1]
    ::iota(::begin(hosts), ::end(hosts), 0);
    /*
     * Shuffle hosts to balance addr lookups to all hosts
     * Too many concurrent lookups send to same host could overwhelm the server, returning error when addr lookup
     */
    ::random_device rd; // obtain a random number from hardware
    ::mt19937 g(rd()); // seed the random generator
    ::shuffle(hosts.begin(), hosts.end(), g); // Shuffle hosts vector
    // lookup addresses and put abstract server addresses into rpc_addresses
    for (auto& host : hosts) {
        string remote_addr;
        hg_addr_t svr_addr = HG_ADDR_NULL;
        auto hostname = CTX->fs_conf()->hosts.at(host) + HOSTNAME_SUFFIX;
        // get the ip address from /etc/hosts which is mapped to the sys_hostfile map
        if (CTX->fs_conf()->sys_hostfile.count(hostname) == 1) {
            auto remote_ip = CTX->fs_conf()->sys_hostfile.at(hostname);
            remote_addr = RPC_PROTOCOL + "://"s + remote_ip + ":"s + CTX->fs_conf()->rpc_port;
        }
        // fallback hostname to use for lookup
        if (remote_addr.empty()) {
            remote_addr = RPC_PROTOCOL + "://"s + hostname + ":"s +
                          CTX->fs_conf()->rpc_port; // convert hostid to remote_addr and port
        }
        CTX->log()->trace("generated remote_addr {} for hostname {} with rpc_port {}",
                         remote_addr, hostname, CTX->fs_conf()->rpc_port);
        // try to look up 3 times before erroring out
        hg_return_t ret;
        for (uint32_t i = 0; i < 4; i++) {
            ret = margo_addr_lookup(ld_margo_rpc_id, remote_addr.c_str(), &svr_addr);
            if (ret != HG_SUCCESS) {
                // still not working after 5 tries.
                if (i == 4) {
                    CTX->log()->error("{}() Unable to lookup address {} from host {}", __func__,
                                     remote_addr, CTX->fs_conf()->hosts.at(CTX->fs_conf()->host_id));
                    return false;
                }
                // Wait a random amount of time and try again
                ::mt19937 eng(rd()); // seed the random generator
                ::uniform_int_distribution<> distr(50, 50 * (i + 2)); // define the range
                ::this_thread::sleep_for(std::chrono::milliseconds(distr(eng)));
            } else {
                break;
            }
        }
        if (svr_addr == HG_ADDR_NULL) {
            CTX->log()->error("{}() looked up address is NULL for address {} from host {}", __func__,
                             remote_addr, CTX->fs_conf()->hosts.at(CTX->fs_conf()->host_id));
            return false;
        }
        rpc_addresses.insert(make_pair(host, svr_addr));
    }
    return true;
}

/**
 * Retrieve abstract svr address handle for hostid
 * @param hostid
 * @param svr_addr
 * @return
 */
bool get_addr_by_hostid(const uint64_t hostid, hg_addr_t& svr_addr) {
    auto address_lookup = rpc_addresses.find(hostid);
    auto found = address_lookup != rpc_addresses.end();
    if (found) {
        svr_addr = address_lookup->second;
        CTX->log()->trace("{}() RPC address lookup success with hostid {}", __func__, address_lookup->first);
        return true;
    } else {
        // not found, unexpected host.
        // This should not happen because all addresses are looked when the environment is initialized.
        CTX->log()->error("{}() Unexpected host id {}. Not found in RPC address cache", __func__, hostid);
        assert(found && "Unexpected host id for rpc address lookup. ID was not found in RPC address cache.");
    }
    return false;
}

/**
 * Determines if the recipient id in an RPC is refering to the local or an remote node
 * @param recipient
 * @return
 */
bool is_local_op(const size_t recipient) {
    return recipient == CTX->fs_conf()->host_id;
}

inline hg_return
margo_create_wrap_helper(const hg_id_t ipc_id, const hg_id_t rpc_id, const size_t recipient, hg_handle_t& handle,
                         bool force_rpc) {
    hg_return_t ret;
    if (is_local_op(recipient) && !force_rpc) { // local
        ret = margo_create(ld_margo_ipc_id, daemon_svr_addr, ipc_id, &handle);
        CTX->log()->debug("{}() to local daemon (IPC)", __func__);
    } else { // remote
        hg_addr_t svr_addr = HG_ADDR_NULL;
        if (!get_addr_by_hostid(recipient, svr_addr)) {
            CTX->log()->error("{}() server address not resolvable for host id {}", __func__, recipient);
            return HG_OTHER_ERROR;
        }
        ret = margo_create(ld_margo_rpc_id, svr_addr, rpc_id, &handle);
        CTX->log()->debug("{}() to remote daemon (RPC)", __func__);
    }
    if (ret != HG_SUCCESS) {
        CTX->log()->error("{}() creating handle FAILED", __func__);
        return HG_OTHER_ERROR;
    }
    return ret;
}

/**
 * Wraps certain margo functions to create a Mercury handle
 * @param ipc_id
 * @param rpc_id
 * @param path
 * @param handle
 * @return
 */
hg_return margo_create_wrap(const hg_id_t ipc_id, const hg_id_t rpc_id,
                            const std::string& path, hg_handle_t& handle,
                            bool force_rpc) {
    auto recipient = CTX->distributor()->locate_file_metadata(path);
    return margo_create_wrap_helper(ipc_id, rpc_id, recipient, handle, force_rpc);
}

/**
 * Wraps certain margo functions to create a Mercury handle
 * @param ipc_id
 * @param rpc_id
 * @param recipient
 * @param handle
 * @param svr_addr
 * @return
 */
hg_return margo_create_wrap(const hg_id_t ipc_id, const hg_id_t rpc_id,
                            const size_t& recipient, hg_handle_t& handle,
                            bool force_rpc) {
    return margo_create_wrap_helper(ipc_id, rpc_id, recipient, handle, force_rpc);
}