
#include <daemon/adafs_ops/data.hpp>

using namespace std;

std::string path_to_fspath(const std::string& path) {
    // root path is absolute as is the path comes in here which is hierarchically under root_path
    // XXX check if this can be done easier
    string fs_path;
    set_difference(path.begin(), path.end(), ADAFS_DATA->mountdir().begin(), ADAFS_DATA->mountdir().end(),
                   std::back_inserter(fs_path));
    if (fs_path.at(0) == '/') {
        fs_path = fs_path.substr(1, fs_path.size());
    }
    // replace / with : to avoid making a bunch of mkdirs to store the data in the underlying fs. XXX Can this be done with hashing?
    replace(fs_path.begin(), fs_path.end(), '/', ':');
    return fs_path;
}

/**
 * Creates the directory in the chunk dir for a file to hold data
 * @param inode
 * @return
 */
// XXX this might be just a temp function as long as we don't use chunks
// XXX this function creates not only the chunk folder but also a single file which holds the data of the 'real' file
int init_chunk_space(const std::string& path) {
    auto fs_path = path_to_fspath(path);

    auto chnk_path = bfs::path(ADAFS_DATA->chunk_path());
    chnk_path /= fs_path;

    // create chunk dir
    bfs::create_directories(chnk_path);

    // XXX create temp big file. remember also to modify the return value
    chnk_path /= "data"s;
    bfs::ofstream ofs{chnk_path};

//    return static_cast<int>(bfs::exists(chnk_path));
    return 0;
}
/**
 * Remove the directory in the chunk dir of a file.
 * @param inode
 * @return
 */
// XXX this might be just a temp function as long as we don't use chunks
int destroy_chunk_space(const std::string& path) {
    auto fs_path = path_to_fspath(path);

    auto chnk_path = bfs::path(ADAFS_DATA->chunk_path());
    chnk_path /= fs_path;

    // create chunk dir
    bfs::remove_all(chnk_path);

//    return static_cast<int>(!bfs::exists(chnk_path));
    return 0;
}

/**
 * Used by an argobots threads. Argument args has the following fields:
 * const std::string* path;
   const char* buf;
   const rpc_chnk_id_t* chnk_id;
   size_t size;
   off64_t off;
   ABT_eventual* eventual;
 * This function is driven by the IO pool. so there is a maximum allowed number of concurrent IO operations per daemon.
 * This function is called by tasklets, as this function cannot be allowed to block.
 * @return written_size<size_t> is put into eventual and returned that way
 */
void write_file_abt(void* _arg) {
    size_t write_size = 0;
    // Unpack args
    auto* arg = static_cast<struct write_chunk_args*>(_arg);
    auto fs_path = path_to_fspath(*arg->path);
    auto chnk_path = bfs::path(ADAFS_DATA->chunk_path());
    chnk_path /= fs_path;
    bfs::create_directories(chnk_path);
    chnk_path /= fmt::FormatInt(arg->chnk_id).c_str();
    // open file
    int fd = open(chnk_path.c_str(), O_WRONLY | O_CREAT, 0640);
    if (fd < 0) {
        write_size = static_cast<size_t>(EIO);
        ABT_eventual_set(arg->eventual, &write_size, sizeof(size_t));
        return;
    }
    // write file
    auto err = pwrite64(fd, arg->buf, arg->size, arg->off);
    if (err < 0) {
        ADAFS_DATA->spdlogger()->error("{}() Error {} while pwriting file {} chunk_id {} size {} off {}", __func__,
                                       strerror(errno), chnk_path.c_str(), arg->chnk_id, arg->size, arg->off);
    } else {
        write_size = static_cast<size_t>(err); // This is cast safe
    }
    ABT_eventual_set(arg->eventual, &write_size, sizeof(size_t));
    // file is closed
    close(fd);
}

int write_chunks(const string& path, const vector<void*>& buf_ptrs, const vector<hg_size_t>& buf_sizes,
                 const off64_t offset, size_t& write_size) {
    write_size = 0;
    // buf sizes also hold chnk ids. we only want to keep calculate the actual chunks
    auto chnk_n = static_cast<unsigned int>(buf_sizes.size() / 2); // Case-safe: There never are so many chunks at once
    vector<ABT_eventual> eventuals(chnk_n);
    vector<unique_ptr<struct write_chunk_args>> thread_args(chnk_n);
    for (unsigned int i = 0; i < chnk_n; i++) {
        auto chnk_id = *(static_cast<size_t*>(buf_ptrs[i]));
        auto chnk_ptr = static_cast<char*>(buf_ptrs[i + chnk_n]);
        auto chnk_size = buf_sizes[i + chnk_n];
        // Starting tasklets for parallel I/O
        ABT_eventual_create(sizeof(size_t), &eventuals[i]); // written file return value
        auto args = make_unique<write_chunk_args>();
        args->path = &path;
        args->buf = chnk_ptr;
        args->chnk_id = chnk_id;
        args->size = chnk_size;
        // only the first chunk gets the offset. the chunks are sorted on the client side
        args->off = (i == 0 ? offset : 0);
        args->eventual = eventuals[i];
        thread_args[i] = std::move(args);
        auto ret = ABT_task_create(RPC_DATA->io_pool(), write_file_abt, &(*thread_args[i]), nullptr);
        if (ret != ABT_SUCCESS) {
            ADAFS_DATA->spdlogger()->error("{}() task create failed", __func__);
        }
    }
    for (unsigned int i = 0; i < chnk_n; i++) {
        size_t* task_written_size;
        // wait causes the calling ult to go into BLOCKED state, implicitly yielding to the pool scheduler
        ABT_eventual_wait(eventuals[i], (void**) &task_written_size);
        if (task_written_size == nullptr || *task_written_size == 0) {
            ADAFS_DATA->spdlogger()->error("{}() Writing file task {} did return nothing. NO ACTION WAS DONE",
                                           __func__, i);
//            // TODO How do we handle already written chunks? Ideally, we would need to remove them after failure.
//            ADAFS_DATA->spdlogger()->error("{}() Writing chunk failed with path {} and id {}. Aborting ...", __func__,
//                                           path, chnk_id);
            write_size = 0;
            return -1;
        } else {
            write_size += *task_written_size;
        }
        ABT_eventual_free(&eventuals[i]);
    }
    return 0;
}

/**
 * Used by an argobots threads. Argument args has the following fields:
 * const std::string* path;
   char* buf;
   const rpc_chnk_id_t* chnk_id;
   size_t size;
   off64_t off;
   ABT_eventual* eventual;
 * This function is driven by the IO pool. so there is a maximum allowed number of concurrent IO operations per daemon.
 * This function is called by tasklets, as this function cannot be allowed to block.
 * @return read_size<size_t> is put into eventual and returned that way
 */
void read_file_abt(void* _arg) {
    size_t read_size = 0;
    //unpack args
    auto* arg = static_cast<struct read_chunk_args*>(_arg);
    auto fs_path = path_to_fspath(*arg->path);
    auto chnk_path = bfs::path(ADAFS_DATA->chunk_path());
    chnk_path /= fs_path;
    chnk_path /= fmt::FormatInt(arg->chnk_id).c_str();;

    int fd = open(chnk_path.c_str(), R_OK);
    if (fd < 0) {
        read_size = static_cast<size_t>(EIO);
        ABT_eventual_set(arg->eventual, &read_size, sizeof(size_t));
        return;
    }
    auto err = pread64(fd, arg->buf, arg->size, arg->off);
    if (err < 0) {
        ADAFS_DATA->spdlogger()->error("{}() Error {} while preading file {} chunk_id {} size {} off {}", __func__,
                                       strerror(errno), chnk_path.c_str(), arg->chnk_id, arg->size, arg->off);
    } else {
        read_size = static_cast<size_t>(err); // This is cast safe
    }
    close(fd);
    ABT_eventual_set(arg->eventual, &read_size, sizeof(size_t));
}

int read_chunks(const string& path, const off64_t offset, const vector<void*>& buf_ptrs,
                const vector<hg_size_t>& buf_sizes,
                size_t& read_size) {
    read_size = 0;
    // buf sizes also hold chnk ids. we only want to keep calculate the actual chunks
    auto chnk_n = static_cast<unsigned int>(buf_sizes.size() / 2); // Case-safe: There never are so many chunks at once
    vector<ABT_eventual> eventuals(chnk_n);
    vector<unique_ptr<struct read_chunk_args>> thread_args(chnk_n);
    for (size_t i = 0; i < chnk_n; i++) {
        auto chnk_id = *(static_cast<size_t*>(buf_ptrs[i]));
        auto chnk_ptr = static_cast<char*>(buf_ptrs[i + chnk_n]);
        auto chnk_size = buf_sizes[i + chnk_n];
        // Starting tasklets for parallel I/O
        ABT_eventual_create(sizeof(size_t), &eventuals[i]); // written file return value
        auto args = make_unique<read_chunk_args>();
        args->path = &path;
        args->buf = chnk_ptr;
        args->chnk_id = chnk_id;
        args->size = chnk_size;
        // only the first chunk gets the offset. the chunks are sorted on the client side
        args->off = (i == 0 ? offset : 0);
        args->eventual = eventuals[i];
        thread_args[i] = std::move(args);
        auto ret = ABT_task_create(RPC_DATA->io_pool(), read_file_abt, &(*thread_args[i]), nullptr);
        if (ret != ABT_SUCCESS) {
            ADAFS_DATA->spdlogger()->error("{}() task create failed", __func__);
        }
    }

    for (unsigned int i = 0; i < chnk_n; i++) {
        size_t* task_read_size;
        ABT_eventual_wait(eventuals[i], (void**) &task_read_size);
        if (task_read_size == nullptr || *task_read_size == 0) {
            ADAFS_DATA->spdlogger()->error("{}() Reading file task {} did return nothing. NO ACTION WAS DONE",
                                           __func__, i);
//            // TODO How do we handle errors?
//            ADAFS_DATA->spdlogger()->error("{}() read chunk failed with path {} and id {}. Aborting ...", __func__,
//                                           path, chnk_id);
            read_size = 0;
            return -1;
        } else {
            read_size += *task_read_size;
        }
        ABT_eventual_free(&eventuals[i]);
    }
    return 0;
}