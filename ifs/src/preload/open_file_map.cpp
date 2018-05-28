#include <global/global_defs.hpp>
#include <preload/open_file_map.hpp>
#include <preload/open_dir.hpp>
#include <preload/preload.hpp>
#include <preload/preload_util.hpp>

using namespace std;

OpenFile::OpenFile(const string& path, const int flags) : path_(path) {
    // set flags to OpenFile
    if (flags & O_CREAT)
        flags_[to_underlying(OpenFile_flags::creat)] = true;
    if (flags & O_APPEND)
        flags_[to_underlying(OpenFile_flags::append)] = true;
    if (flags & O_TRUNC)
        flags_[to_underlying(OpenFile_flags::trunc)] = true;
    if (flags & O_RDONLY)
        flags_[to_underlying(OpenFile_flags::rdonly)] = true;
    if (flags & O_WRONLY)
        flags_[to_underlying(OpenFile_flags::wronly)] = true;
    if (flags & O_RDWR)
        flags_[to_underlying(OpenFile_flags::rdwr)] = true;

    pos_ = 0; // If O_APPEND flag is used, it will be used before each write.
}

OpenFileMap::OpenFileMap() {}

OpenFile::~OpenFile() {

}

string OpenFile::path() const {
    return path_;
}

void OpenFile::path(const string& path_) {
    OpenFile::path_ = path_;
}

off64_t OpenFile::pos() {
    lock_guard<mutex> lock(pos_mutex_);
    return pos_;
}

void OpenFile::pos(off64_t pos_) {
    lock_guard<mutex> lock(pos_mutex_);
    OpenFile::pos_ = pos_;
}

const bool OpenFile::get_flag(OpenFile_flags flag) {
    lock_guard<mutex> lock(pos_mutex_);
    return flags_[to_underlying(flag)];
}

void OpenFile::set_flag(OpenFile_flags flag, bool value) {
    lock_guard<mutex> lock(flag_mutex_);
    flags_[to_underlying(flag)] = value;
}

// OpenFileMap starts here

shared_ptr<OpenFile> OpenFileMap::get(int fd) {
    lock_guard<recursive_mutex> lock(files_mutex_);
    auto f = files_.find(fd);
    if (f == files_.end()) {
        return nullptr;
    } else {
        return f->second;
    }
}

shared_ptr<OpenDir> OpenFileMap::get_dir(int dirfd) {
    auto f = get(dirfd);
    if(f == nullptr){
        return nullptr;
    }
    auto open_dir = static_pointer_cast<OpenDir>(f);
    // If open_file is not an OpenDir we are returning nullptr
    return open_dir;
}

bool OpenFileMap::exist(const int fd) {
    lock_guard<recursive_mutex> lock(files_mutex_);
    auto f = files_.find(fd);
    return !(f == files_.end());
}

int OpenFileMap::safe_generate_fd_idx_() {
    auto fd = generate_fd_idx();
    /*
     * Check if fd is still in use and generate another if yes
     * Note that this can only happen once the all fd indices within the int has been used to the int::max
     * Once this limit is exceeded, we set fd_idx back to 3 and begin anew. Only then, if a file was open for
     * a long time will we have to generate another index.
     *
     * This situation can only occur when all fd indices have been given away once and we start again,
     * in which case the fd_validation_needed flag is set. fd_validation is set to false, if
     */
    if (fd_validation_needed) {
        while (exist(fd)) {
            fd = generate_fd_idx();
        }
    }
    return fd;
}

int OpenFileMap::add(std::shared_ptr<OpenFile> open_file) {
    auto fd = safe_generate_fd_idx_();
    lock_guard<recursive_mutex> lock(files_mutex_);
    files_.insert(make_pair(fd, open_file));
    return fd;
}

bool OpenFileMap::remove(const int fd) {
    lock_guard<recursive_mutex> lock(files_mutex_);
    auto f = files_.find(fd);
    if (f == files_.end()) {
        return false;
    }
    files_.erase(fd);
    if (fd_validation_needed && files_.empty()) {
        fd_validation_needed = false;
        CTX->log()->info("{}() fd_validation flag reset", __func__);
    }
    return true;
}

int OpenFileMap::dup(const int oldfd) {
    lock_guard<recursive_mutex> lock(files_mutex_);
    auto open_file = get(oldfd);
    if (open_file == nullptr) {
        errno = EBADF;
        return -1;
    }
    auto newfd = safe_generate_fd_idx_();
    files_.insert(make_pair(newfd, open_file));
    return newfd;
}

int OpenFileMap::dup2(const int oldfd, const int newfd) {
    lock_guard<recursive_mutex> lock(files_mutex_);
    auto open_file = get(oldfd);
    if (open_file == nullptr) {
        errno = EBADF;
        return -1;
    }
    if (oldfd == newfd)
        return newfd;
    // remove newfd if exists in filemap silently
    if (exist(newfd)) {
        remove(newfd);
    }
    // to prevent duplicate fd idx in the future. First three fd are reservered by os streams that we do not overwrite
    if (get_fd_idx() < newfd && newfd != 0 && newfd != 1 && newfd != 2)
        fd_validation_needed = true;
    files_.insert(make_pair(newfd, open_file));
    return newfd;
}
