
#ifndef IFS_OPEN_FILE_MAP_HPP
#define IFS_OPEN_FILE_MAP_HPP

#include <map>
#include <mutex>
#include <memory>

/* Forward declaration */
class OpenDir;


enum class OpenFile_flags {
    append = 0,
    creat,
    trunc,
    rdonly,
    wronly,
    rdwr,
    cloexec,
    flag_count // this is purely used as a size variable of this enum class
};

enum class FileType {
    regular,
    directory
};

class OpenFile {
protected:
    std::string path_;
    std::array<bool, static_cast<int>(OpenFile_flags::flag_count)> flags_ = {false};
    off64_t pos_;
    std::mutex pos_mutex_;
    std::mutex flag_mutex_;

public:
    // multiple threads may want to update the file position if fd has been duplicated by dup()

    OpenFile(const std::string& path, int flags);

    ~OpenFile();

    // getter/setter
    std::string path() const;

    void path(const std::string& path_);

    off64_t pos();

    void pos(off64_t pos_);

    const bool get_flag(OpenFile_flags flag);

    void set_flag(OpenFile_flags flag, bool value);

};


class OpenFileMap {

private:
    std::map<int, std::shared_ptr<OpenFile>> files_;
    std::recursive_mutex files_mutex_;

    int safe_generate_fd_idx_();

public:
    OpenFileMap();

    std::shared_ptr<OpenFile> get(int fd);

    std::shared_ptr<OpenDir> get_dir(int dirfd);

    bool exist(int fd);

    int add(std::shared_ptr<OpenFile>);

    bool remove(int fd);

    int dup(int oldfd);

    int dup2(int oldfd, int newfd);

};


#endif //IFS_OPEN_FILE_MAP_HPP
