#include <dlfcn.h>

#include <preload/preload.hpp>
#include <preload/ipc_types.hpp>
#include <preload/margo_ipc.hpp>
#include <preload/rpc/ld_rpc_data_ws.hpp>
#include <preload/passthrough.hpp>

enum class Margo_mode {
    RPC, IPC
};

// atomic bool to check for margo initialization XXX This has to become more robus
std::atomic<bool> is_env_initialized(false);

// external variables that are initialized here
// IPC IDs
hg_id_t ipc_minimal_id;
hg_id_t ipc_config_id;
hg_id_t ipc_mk_node_id;
hg_id_t ipc_access_id;
hg_id_t ipc_stat_id;
hg_id_t ipc_rm_node_id;
hg_id_t ipc_update_metadentry_id;
hg_id_t ipc_update_metadentry_size_id;
hg_id_t ipc_write_data_id;
hg_id_t ipc_read_data_id;
// RPC IDs
hg_id_t rpc_minimal_id;
hg_id_t rpc_mk_node_id;
hg_id_t rpc_access_id;
hg_id_t rpc_stat_id;
hg_id_t rpc_rm_node_id;
hg_id_t rpc_update_metadentry_id;
hg_id_t rpc_update_metadentry_size_id;
hg_id_t rpc_write_data_id;
hg_id_t rpc_read_data_id;
// Margo instances
margo_instance_id ld_margo_ipc_id;
margo_instance_id ld_margo_rpc_id;
// rpc address cache
KVCache rpc_address_cache{32768, 4096}; // XXX Set values are not based on anything...
// local daemon IPC address
hg_addr_t daemon_svr_addr = HG_ADDR_NULL;

/**
 * Initializes the Argobots environment
 * @return
 */
bool init_ld_argobots() {
    ld_logger->info("{}() Initializing Argobots ...", __func__);

    // We need no arguments to init
    auto argo_err = ABT_init(0, nullptr);
    if (argo_err != 0) {
        ld_logger->info("{}() ABT_init() Failed to init Argobots (client)", __func__);
        return false;
    }
    // Set primary execution stream to idle without polling. Normally xstreams cannot sleep. This is what ABT_snoozer does
    argo_err = ABT_snoozer_xstream_self_set();
    if (argo_err != 0) {
        ld_logger->info("{}() ABT_snoozer_xstream_self_set()  (client)", __func__);
        return false;
    }
    ld_logger->info("{}() Argobots initialization successful.", __func__);
    return true;
}

/**
 * Registers a margo instance with all used RPC, differentiating between IPC and RPC client
 * Note that the rpc tags are redundant for rpc and ipc ids
 * @param mid
 * @param mode
 */
void register_client_rpcs(margo_instance_id mid, Margo_mode mode) {
    if (mode == Margo_mode::IPC) {
        // IPC IDs
        ipc_config_id = MARGO_REGISTER(mid, hg_tag::fs_config, ipc_config_in_t, ipc_config_out_t,
                                       NULL);
        ipc_minimal_id = MARGO_REGISTER(mid, hg_tag::minimal, rpc_minimal_in_t, rpc_minimal_out_t, NULL);
        ipc_mk_node_id = MARGO_REGISTER(mid, hg_tag::create, rpc_mk_node_in_t, rpc_err_out_t, NULL);
        ipc_access_id = MARGO_REGISTER(mid, hg_tag::access, rpc_access_in_t, rpc_err_out_t, NULL);
        ipc_stat_id = MARGO_REGISTER(mid, hg_tag::stat, rpc_stat_in_t, rpc_stat_out_t, NULL);
        ipc_rm_node_id = MARGO_REGISTER(mid, hg_tag::remove, rpc_rm_node_in_t,
                                        rpc_err_out_t, NULL);
        ipc_update_metadentry_id = MARGO_REGISTER(mid, hg_tag::update_metadentry, rpc_update_metadentry_in_t,
                                                  rpc_err_out_t, NULL);
        ipc_update_metadentry_size_id = MARGO_REGISTER(mid, hg_tag::update_metadentry_size,
                                                       rpc_update_metadentry_size_in_t,
                                                       rpc_update_metadentry_size_out_t,
                                                       NULL);
        ipc_write_data_id = MARGO_REGISTER(mid, hg_tag::write_data, rpc_write_data_in_t, rpc_data_out_t,
                                           NULL);
        ipc_read_data_id = MARGO_REGISTER(mid, hg_tag::read_data, rpc_read_data_in_t, rpc_data_out_t,
                                          NULL);
    } else {
        // RPC IDs
        rpc_minimal_id = MARGO_REGISTER(mid, hg_tag::minimal, rpc_minimal_in_t, rpc_minimal_out_t, NULL);
        rpc_mk_node_id = MARGO_REGISTER(mid, hg_tag::create, rpc_mk_node_in_t, rpc_err_out_t, NULL);
        rpc_access_id = MARGO_REGISTER(mid, hg_tag::access, rpc_access_in_t, rpc_err_out_t, NULL);
        rpc_stat_id = MARGO_REGISTER(mid, hg_tag::stat, rpc_stat_in_t, rpc_stat_out_t, NULL);
        rpc_rm_node_id = MARGO_REGISTER(mid, hg_tag::remove, rpc_rm_node_in_t,
                                        rpc_err_out_t, NULL);
        rpc_update_metadentry_id = MARGO_REGISTER(mid, hg_tag::update_metadentry, rpc_update_metadentry_in_t,
                                                  rpc_err_out_t, NULL);
        rpc_update_metadentry_size_id = MARGO_REGISTER(mid, hg_tag::update_metadentry_size,
                                                       rpc_update_metadentry_size_in_t,
                                                       rpc_update_metadentry_size_out_t,
                                                       NULL);
        rpc_write_data_id = MARGO_REGISTER(mid, hg_tag::write_data, rpc_write_data_in_t, rpc_data_out_t,
                                           NULL);
        rpc_read_data_id = MARGO_REGISTER(mid, hg_tag::read_data, rpc_read_data_in_t, rpc_data_out_t,
                                          NULL);
    }
}

/**
 * Initializes the Margo client for a given na_plugin
 * @param mode
 * @param na_plugin
 * @return
 */
bool init_margo_client(Margo_mode mode, const string na_plugin) {

    ABT_xstream xstream = ABT_XSTREAM_NULL;
    ABT_pool pool = ABT_POOL_NULL;

    // get execution stream and its main pools
    auto ret = ABT_xstream_self(&xstream);
    if (ret != ABT_SUCCESS)
        return false;
    ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
    if (ret != ABT_SUCCESS) return false;
    if (mode == Margo_mode::IPC)
        ld_logger->info("{}() Initializing Mercury IPC client ...", __func__);
    else
        ld_logger->info("{}() Initializing Mercury RPC client ...", __func__);
    /* MERCURY PART */
    // Init Mercury layer (must be finalized when finished)
    hg_class_t* hg_class;
    hg_context_t* hg_context;
    hg_class = HG_Init(na_plugin.c_str(), HG_FALSE);
    if (hg_class == nullptr) {
        ld_logger->info("{}() HG_Init() Failed to init Mercury client layer", __func__);
        return false;
    }
    // Create a new Mercury context (must be destroyed when finished)
    hg_context = HG_Context_create(hg_class);
    if (hg_context == nullptr) {
        ld_logger->info("{}() HG_Context_create() Failed to create Mercury client context", __func__);
        HG_Finalize(hg_class);
        return false;
    }
    ld_logger->info("{}() Mercury initialized.", __func__);

    /* MARGO PART */
    if (mode == Margo_mode::IPC)
        ld_logger->info("{}() Initializing Margo IPC client ...", __func__);
    else
        ld_logger->info("{}() Initializing Margo RPC client ...", __func__);
    // margo will run in the context of thread
    auto mid = margo_init_pool(pool, pool, hg_context);
    if (mid == MARGO_INSTANCE_NULL) {
        ld_logger->error("{}() margo_init_pool failed to initialize the Margo client", __func__);
        return false;
    }
    ld_logger->info("{}() Margo initialized.", __func__);

    if (mode == Margo_mode::IPC) {
        ld_margo_ipc_id = mid;
        auto adafs_daemon_pid = getProcIdByName("adafs_daemon"s);
        if (adafs_daemon_pid == -1) {
            ld_logger->error("{}() ADA-FS daemon not started. Exiting ...", __func__);
            return false;
        }
        ld_logger->info("{}() ADA-FS daemon with PID {} found.", __func__, adafs_daemon_pid);

        string sm_addr_str = "na+sm://"s + to_string(adafs_daemon_pid) + "/0";
        margo_addr_lookup(ld_margo_ipc_id, sm_addr_str.c_str(), &daemon_svr_addr);
    } else
        ld_margo_rpc_id = mid;

    register_client_rpcs(mid, mode);
#ifdef MARGODIAG
    margo_diag_start(mid);
#endif
    return true;
}

/**
 * Returns atomic bool, if Margo is running
 * @return
 */
bool ld_is_env_initialized() {
    return is_env_initialized;
}

/**
 * This function is only called in the preload constructor and initializes Argobots and Margo clients
 */
bool init_environment() {
    if (!init_ld_argobots()) {
        ld_logger->error("{}() Unable to initialize Argobots.", __func__);
        return false;
    }
    if (!init_margo_client(Margo_mode::IPC, "na+sm"s)) {
        ld_logger->error("{}() Unable to initialize Margo IPC client.", __func__);
        return false;
    }
    if (!ipc_send_get_fs_config()) {
        ld_logger->error("{}() Unable to fetch file system configurations from daemon process through IPC.", __func__);
        return false;
    }
    if (!init_margo_client(Margo_mode::RPC, RPC_PROTOCOL)) {
        ld_logger->error("{}() Unable to initialize Margo RPC client.", __func__);
        return false;
    }
    is_env_initialized = true;
    ld_logger->info("{}() Environment initialization successful.", __func__);
    return true;
}

/**
 * Called initially when preload library is used with the LD_PRELOAD environment variable
 */
void init_preload() {
    init_passthrough_if_needed();
    if (!init_environment()) {
        ld_logger->error("{}() Failed to initialize client environment", __func__);
        exit(EXIT_FAILURE);
    }
}

/**
 * Called last when preload library is used with the LD_PRELOAD environment variable
 */
void destroy_preload() {
    auto services_used = (ld_margo_ipc_id != nullptr || ld_margo_rpc_id != nullptr);
#ifdef MARGODIAG
    if (ld_margo_ipc_id != nullptr) {
        cout << "\n####################\n\nMargo IPC client stats: " << endl;
        margo_diag_dump(ld_margo_ipc_id, "-", 0);
    }
    if (ld_margo_rpc_id != nullptr) {
        cout << "\n####################\n\nMargo RPC client stats: " << endl;
        margo_diag_dump(ld_margo_rpc_id, "-", 0);
    }
#endif
    // Shut down RPC client if used
    if (ld_margo_rpc_id != nullptr) {
        // free all rpc addresses in LRU map and finalize margo rpc
        ld_logger->info("{}() Freeing Margo RPC svr addresses ...", __func__);
        auto free_all_addr = [&](const KVCache::node_type& n) {
            if (margo_addr_free(ld_margo_rpc_id, n.value) != HG_SUCCESS) {
                ld_logger->warn("{}() Unable to free RPC client's svr address: {}.", __func__, n.key);
            }
        };
        rpc_address_cache.cwalk(free_all_addr);
        ld_logger->info("{}() About to finalize the margo client", __func__);
        margo_finalize(ld_margo_rpc_id);
        ld_logger->info("{}() Shut down Margo RPC client successful", __func__);
    }
    // Shut down IPC client if used
    if (ld_margo_ipc_id != nullptr) {
        ld_logger->info("{}() Freeing Margo IPC daemon svr address ...", __func__);
        if (margo_addr_free(ld_margo_ipc_id, daemon_svr_addr) != HG_SUCCESS)
            ld_logger->warn("{}() Unable to free IPC client's daemon svr address.", __func__);
        margo_finalize(ld_margo_ipc_id);
        ld_logger->info("{}() Shut down Margo IPC client successful", __func__);
    }
    if (services_used)
        ld_logger->info("All services shut down. Client shutdown complete.");
}