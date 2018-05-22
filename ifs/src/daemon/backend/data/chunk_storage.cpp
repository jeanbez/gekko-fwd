#include <daemon/backend/data/chunk_storage.hpp>
#include <global/path_util.hpp>

#include <cerrno>
#include <extern/spdlog/spdlog.h>
#include <boost/filesystem.hpp>

namespace bfs = boost::filesystem;

std::string ChunkStorage::absolute(const std::string& internal_path) const {
    assert(is_relative_path(internal_path));
    return root_path + '/' + internal_path;
}

ChunkStorage::ChunkStorage(const std::string& path, const size_t chunksize) :
    root_path(path),
    chunksize(chunksize)
{
    //TODO check path: absolute, exists, permission to write etc...
    assert(is_absolute_path(root_path));

    /* Initialize logger */
    log = spdlog::get(LOGGER_NAME);
    assert(log);

    log->debug("Chunk storage initialized with path: '{}'", root_path);
}

std::string ChunkStorage::get_chunks_dir(const std::string& file_path) {
    assert(is_absolute_path(file_path));
    std::string chunk_dir = file_path.substr(1);
    std::replace(chunk_dir.begin(), chunk_dir.end(), '/', ':');
    return chunk_dir;
}

std::string ChunkStorage::get_chunk_path(const std::string& file_path, unsigned int chunk_id) {
    return get_chunks_dir(file_path) + std::to_string(chunk_id);
}

void ChunkStorage::destroy_chunk_space(const std::string& file_path) const {
    auto chunk_dir = absolute(get_chunks_dir(file_path));
    try {
        bfs::remove_all(chunk_dir);
    } catch (const bfs::filesystem_error& e){
        log->error("Failed to remove chunk directory. Path: '{}', Error: {}", chunk_dir, e.what());
    }
}

void ChunkStorage::init_chunk_space(const std::string& file_path) const {
    auto chunk_dir = absolute(get_chunks_dir(file_path));
    auto err = mkdir(chunk_dir.c_str(), 0640);
    if(err == -1 && errno != EEXIST){
        log->error("Failed to create chunk dir. Path: {}, Error: {}", chunk_dir, std::strerror(errno));
        throw std::system_error(errno, std::system_category(), "Failed to create chunk directory");
    }
}

void ChunkStorage::write_chunk(const std::string& file_path, unsigned int chunk_id,
        const char * buff, size_t size, off64_t offset, ABT_eventual& eventual) const {

    assert((offset + size) <= chunksize);

    init_chunk_space(file_path);

    auto chunk_path = absolute(get_chunk_path(file_path, chunk_id));
    int fd = open(chunk_path.c_str(), O_WRONLY | O_CREAT, 0640);
    if(fd < 0) {
        log->error("Failed to open chunk file for write. File: {}, Error: {}", chunk_path, std::strerror(errno));
        throw std::system_error(errno, std::system_category(), "Failed to open chunk file for write");
    }

    auto wrote = pwrite64(fd, buff, size, offset);
    if (wrote < 0) {
        log->error("Failed to write chunk file. File: {}, size: {}, offset: {}, Error: {}",
                chunk_path, size, offset, std::strerror(errno));
        throw std::system_error(errno, std::system_category(), "Failed to write chunk file");
    }

    ABT_eventual_set(eventual, &wrote, sizeof(size_t));

    auto err = close(fd);
    if (err < 0) {
        log->error("Failed to close chunk file after write. File: {}, Error: {}",
                chunk_path, std::strerror(errno));
        //throw std::system_error(errno, std::system_category(), "Failed to close chunk file");
    }
}

void ChunkStorage::read_chunk(const std::string& file_path, unsigned int chunk_id,
        char * buff, size_t size, off64_t offset, ABT_eventual& eventual) const {
    assert((offset + size) <= chunksize);
    auto chunk_path = absolute(get_chunk_path(file_path, chunk_id));
    int fd = open(chunk_path.c_str(), O_RDONLY);
    if(fd < 0) {
        log->error("Failed to open chunk file for read. File: {}, Error: {}", chunk_path, std::strerror(errno));
        throw std::system_error(errno, std::system_category(), "Failed to open chunk file for read");
    }

    auto read = pread64(fd, buff, size, offset);
    if (read < 0) {
        log->error("Failed to read chunk file. File: {}, size: {}, offset: {}, Error: {}",
                chunk_path, size, offset, std::strerror(errno));
        throw std::system_error(errno, std::system_category(), "Failed to read chunk file");
    }

    ABT_eventual_set(eventual, &read, sizeof(size_t));

    auto err = close(fd);
    if (err < 0) {
        log->error("Failed to close chunk file after read. File: {}, Error: {}",
                chunk_path, std::strerror(errno));
        //throw std::system_error(errno, std::system_category(), "Failed to close chunk file");
    }
}