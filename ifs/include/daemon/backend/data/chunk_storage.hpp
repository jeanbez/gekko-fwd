#ifndef IFS_CHUNK_STORAGE_HPP
#define IFS_CHUNK_STORAGE_HPP

#include <abt.h>
#include <string>
#include <memory>

/* Forward declarations */
namespace spdlog {
    class logger;
}


class ChunkStorage {
    private:
        static constexpr const char * LOGGER_NAME = "ChunkStorage";

        std::shared_ptr<spdlog::logger> log;

        std::string root_path;
        size_t chunksize;
        inline std::string absolute(const std::string& internal_path) const;
        static inline std::string get_chunks_dir(const std::string& file_path);
        static inline std::string get_chunk_path(const std::string& file_path, unsigned int chunk_id);
        void init_chunk_space(const std::string& file_path) const;

    public:
        ChunkStorage(const std::string& path, const size_t chunksize);
        void write_chunk(const std::string& file_path, unsigned int chunk_id,
                         const char * buff, size_t size, off64_t offset,
                         ABT_eventual& eventual) const;
        void read_chunk(const std::string& file_path, unsigned int chunk_id,
                         char * buff, size_t size, off64_t offset,
                         ABT_eventual& eventual) const;
        void destroy_chunk_space(const std::string& file_path) const;
};

#endif //IFS_CHUNK_STORAGE_HPP
