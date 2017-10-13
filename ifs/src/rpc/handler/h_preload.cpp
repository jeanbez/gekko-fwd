//
// Created by evie on 9/12/17.
//

#include <rpc/rpc_defs.hpp>
#include <rpc/rpc_utils.hpp>
#include <preload/ipc_types.hpp>

#include <daemon/fs_operations.hpp>
#include <adafs_ops/metadentry.hpp>
#include <db/db_ops.hpp>
#include <adafs_ops/data.hpp>

using namespace std;

static hg_return_t ipc_srv_fs_config(hg_handle_t handle) {
    const struct hg_info* hgi;
    ipc_config_in_t in;
    ipc_config_out_t out;


    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->info("Got config IPC");

    hgi = HG_Get_info(handle);

    auto mid = margo_hg_class_to_instance(hgi->hg_class);

    // get fs config
    out.mountdir = ADAFS_DATA->mountdir().c_str();
    out.rootdir = ADAFS_DATA->rootdir().c_str();
    out.atime_state = static_cast<hg_bool_t>(ADAFS_DATA->atime_state());
    out.mtime_state = static_cast<hg_bool_t>(ADAFS_DATA->mtime_state());
    out.ctime_state = static_cast<hg_bool_t>(ADAFS_DATA->ctime_state());
    out.uid_state = static_cast<hg_bool_t>(ADAFS_DATA->uid_state());
    out.gid_state = static_cast<hg_bool_t>(ADAFS_DATA->gid_state());
    out.inode_no_state = static_cast<hg_bool_t>(ADAFS_DATA->inode_no_state());
    out.link_cnt_state = static_cast<hg_bool_t>(ADAFS_DATA->link_cnt_state());
    out.blocks_state = static_cast<hg_bool_t>(ADAFS_DATA->blocks_state());
    out.uid = getuid();
    out.gid = getgid();
    out.hosts_raw = static_cast<hg_const_string_t>(ADAFS_DATA->hosts_raw().c_str());
    out.host_id = static_cast<hg_uint64_t>(ADAFS_DATA->host_id());
    out.host_size = static_cast<hg_uint64_t>(ADAFS_DATA->host_size());
    ADAFS_DATA->spdlogger()->info("Sending output configs back to library");
    auto hret = margo_respond(mid, handle, &out);
    if (hret != HG_SUCCESS) {
        ADAFS_DATA->spdlogger()->error("Failed to respond to open ipc");
    }

    out.mountdir = nullptr;
    out.rootdir = nullptr;
    out.hosts_raw = nullptr;

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(ipc_srv_fs_config)

static hg_return_t ipc_srv_open(hg_handle_t handle) {
    const struct hg_info* hgi;
    ipc_open_in_t in;
    ipc_err_out_t out;


    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->debug("Got open IPC with path {}", in.path);

    hgi = HG_Get_info(handle);

    auto mid = margo_hg_class_to_instance(hgi->hg_class);

    // do open TODO handle da flags
    string path = in.path;
    out.err = create_metadentry(in.path, in.mode);

    ADAFS_DATA->spdlogger()->debug("Sending output {}", out.err);
    auto hret = margo_respond(mid, handle, &out);
    if (hret != HG_SUCCESS) {
        ADAFS_DATA->spdlogger()->error("Failed to respond to open ipc");
    }

    in.path = nullptr;

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(ipc_srv_open)

static hg_return_t ipc_srv_stat(hg_handle_t handle) {
    const struct hg_info* hgi;
    ipc_stat_in_t in;
    ipc_stat_out_t out;


    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->debug("Got stat IPC with path {}", in.path);

    hgi = HG_Get_info(handle);

    auto mid = margo_hg_class_to_instance(hgi->hg_class);

    // do open
    string val;
    auto err = db_get_metadentry(in.path, val);
    if (err) {
        out.err = 0;
        out.db_val = val.c_str();
    } else {
        out.err = 1;
    }
    ADAFS_DATA->spdlogger()->debug("Sending output {}", out.err);
    auto hret = margo_respond(mid, handle, &out);
    if (hret != HG_SUCCESS) {
        ADAFS_DATA->spdlogger()->error("Failed to respond to stat ipc");
    }

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(ipc_srv_stat)

static hg_return_t srv_update_metadentry(hg_handle_t handle) {
    const struct hg_info* hgi;
    update_metadentry_in_t in;
    ipc_err_out_t out;


    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->debug("Got update metadentry IPC with path {}", in.path);

    hgi = HG_Get_info(handle);

    auto mid = margo_hg_class_to_instance(hgi->hg_class);

    // do update
    Metadata md{};
    auto err = get_metadentry(in.path, md);
    if (err == 0) {
        out.err = 0;
        if (in.inode_no_flag == HG_TRUE)
            md.inode_no(in.inode_no);
        if (in.block_flag == HG_TRUE)
            md.blocks(in.blocks);
        if (in.uid_flag == HG_TRUE)
            md.uid(in.uid);
        if (in.gid_flag == HG_TRUE)
            md.gid(in.gid);
        if (in.nlink_flag == HG_TRUE)
            md.link_count(in.nlink);
        if (in.size_flag == HG_TRUE)
            md.size(in.size);
        if (in.atime_flag == HG_TRUE)
            md.atime(in.atime);
        if (in.mtime_flag == HG_TRUE)
            md.mtime(in.mtime);
        if (in.ctime_flag == HG_TRUE)
            md.ctime(in.ctime);
        out.err = update_metadentry(in.path, md);
    } else {
        out.err = 1;
    }
    ADAFS_DATA->spdlogger()->debug("Sending output {}", out.err);
    auto hret = margo_respond(mid, handle, &out);
    if (hret != HG_SUCCESS) {
        ADAFS_DATA->spdlogger()->error("Failed to respond to update metadentry ipc");
    }

    in.path = nullptr;

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(srv_update_metadentry)

static hg_return_t srv_update_metadentry_size(hg_handle_t handle) {
    const struct hg_info* hgi;
    update_metadentry_size_in_t in;
    ipc_err_out_t out;


    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->debug("Got update metadentry size IPC with path {}", in.path);

    hgi = HG_Get_info(handle);

    auto mid = margo_hg_class_to_instance(hgi->hg_class);

    // do update
    out.err = update_metadentry_size(in.path, in.size);
    ADAFS_DATA->spdlogger()->debug("Sending output {}", out.err);
    auto hret = margo_respond(mid, handle, &out);
    if (hret != HG_SUCCESS) {
        ADAFS_DATA->spdlogger()->error("Failed to respond to update metadentry size ipc");
    }

    in.path = nullptr;

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(srv_update_metadentry_size)

static hg_return_t ipc_srv_unlink(hg_handle_t handle) {
    const struct hg_info* hgi;
    ipc_unlink_in_t in;
    ipc_err_out_t out;


    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->debug("Got unlink IPC with path {}", in.path);

    hgi = HG_Get_info(handle);

    auto mid = margo_hg_class_to_instance(hgi->hg_class);

    // do unlink
    out.err = remove_node(in.path);
    ADAFS_DATA->spdlogger()->debug("Sending output {}", out.err);
    auto hret = margo_respond(mid, handle, &out);
    if (hret != HG_SUCCESS) {
        ADAFS_DATA->spdlogger()->error("Failed to respond to unlink ipc");
    }

    in.path = nullptr;

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(ipc_srv_unlink)

static hg_return_t ipc_srv_write_data(hg_handle_t handle) {
    ipc_write_data_in_t in;
    ipc_data_out_t out;
    void* b_buf;
    hg_bulk_t bulk_handle;

    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->debug("Got write IPC with path {} size {} offset {}", in.path, in.size, in.offset);

    auto hgi = HG_Get_info(handle);
    auto mid = margo_hg_class_to_instance(hgi->hg_class);
    // register local buffer to fill for bulk pull
    auto b_buf_wrap = make_unique<char[]>(in.size);
    b_buf = reinterpret_cast<void*>(b_buf_wrap.get());
    ret = HG_Bulk_create(hgi->hg_class, 1, &b_buf, &in.size, HG_BULK_WRITE_ONLY, &bulk_handle);
    // push data to client
    if (ret == HG_SUCCESS) {
        // pull data from client here
        margo_bulk_transfer(mid, HG_BULK_PULL, hgi->addr, in.bulk_handle, 0, bulk_handle, 0, in.size);
        // do write operation
        auto buf = reinterpret_cast<char*>(b_buf);
        out.res = write_file(in.path, buf, out.io_size, in.size, in.offset, (in.append == HG_TRUE));
        if (out.res != 0) {
            ADAFS_DATA->spdlogger()->error("Failed to write data to local disk.");
            out.io_size = 0;
        }
        HG_Bulk_free(bulk_handle);
    } else {
        ADAFS_DATA->spdlogger()->error("Failed to pull data from client in write operation");
        out.res = EIO;
        out.io_size = 0;
    }
    ADAFS_DATA->spdlogger()->debug("Sending output response {}", out.res);
    auto hret = margo_respond(mid, handle, &out);
    if (hret != HG_SUCCESS) {
        ADAFS_DATA->spdlogger()->error("Failed to respond to write request");
    }

    in.path = nullptr;

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(ipc_srv_write_data)

static hg_return_t ipc_srv_read_data(hg_handle_t handle) {
    ipc_read_data_in_t in;
    ipc_data_out_t out;
    void* b_buf;
    hg_bulk_t bulk_handle;

    auto ret = HG_Get_input(handle, &in);
    assert(ret == HG_SUCCESS);
    ADAFS_DATA->spdlogger()->debug("Got read IPC with path {} size {} offset {}", in.path, in.size, in.offset);

    auto hgi = HG_Get_info(handle);
    auto mid = margo_hg_class_to_instance(hgi->hg_class);

    // set up buffer to read
    auto buf = make_unique<char[]>(in.size);

    out.res = read_file(buf.get(), out.io_size, in.path, in.size, in.offset);

    if (out.res != 0) {
        ADAFS_DATA->spdlogger()->error("Could not open file with path: {}", in.path);
        ADAFS_DATA->spdlogger()->debug("Sending output response {}", out.res);
        auto hret = margo_respond(mid, handle, &out);
        if (hret != HG_SUCCESS) {
            ADAFS_DATA->spdlogger()->error("Failed to respond to read request");
        }
    } else {
        // set up buffer for bulk transfer
        b_buf = (void*) buf.get();

        ret = HG_Bulk_create(hgi->hg_class, 1, &b_buf, &in.size, HG_BULK_READ_ONLY, &bulk_handle);

        // push data to client
        if (ret == HG_SUCCESS)
            margo_bulk_transfer(mid, HG_BULK_PUSH, hgi->addr, in.bulk_handle, 0, bulk_handle, 0, in.size);
        else {
            ADAFS_DATA->spdlogger()->error("Failed to send data to client in read operation");
            out.res = EIO;
            out.io_size = 0;
        }
        ADAFS_DATA->spdlogger()->debug("Sending output response {}", out.res);
        // respond rpc
        auto hret = margo_respond(mid, handle, &out);
        if (hret != HG_SUCCESS) {
            ADAFS_DATA->spdlogger()->error("Failed to respond to read request");
        }
        HG_Bulk_free(bulk_handle);
    }

    in.path = nullptr;

    // Destroy handle when finished
    HG_Free_input(handle, &in);
    HG_Free_output(handle, &out);
    HG_Destroy(handle);
    return HG_SUCCESS;
}

DEFINE_MARGO_RPC_HANDLER(ipc_srv_read_data)