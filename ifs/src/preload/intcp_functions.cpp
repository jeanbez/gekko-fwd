/**
 * All intercepted functions are defined here
 */
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <preload/preload.hpp>
#include <preload/passthrough.hpp>
#include <preload/adafs_functions.hpp>
#include <preload/intcp_functions.hpp>


using namespace std;

int open(const char* path, int flags, ...) {
    init_passthrough_if_needed();

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list vl;
        va_start(vl, flags);
        mode = static_cast<mode_t>(va_arg(vl, int));
        va_end(vl);
    }

    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_open(rel_path, mode, flags);
        }
    }
    return (reinterpret_cast<decltype(&open)>(libc_open))(path, flags, mode);
}

#undef open64

int open64(const char* path, int flags, ...) {
    init_passthrough_if_needed();
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return open(path, flags | O_LARGEFILE, mode);
}

int openat(int dirfd, const char *path, int flags, ...) {
    init_passthrough_if_needed();

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list vl;
        va_start(vl, flags);
        mode = static_cast<mode_t>(va_arg(vl, int));
        va_end(vl);
    }

    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_open(rel_path, mode, flags);
        }

        if (CTX->file_map()->exist(dirfd)) {
            CTX->log()->error("{}() called with relative path: NOT SUPPORTED", __func__);
            errno = ENOTSUP;
            return -1;
        }
    }

    return (reinterpret_cast<decltype(&openat)>(libc_openat))(dirfd, path, flags, mode);
}

int openat64(int dirfd, const char *path, int flags, ...) {
    init_passthrough_if_needed();

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list vl;
        va_start(vl, flags);
        mode = static_cast<mode_t>(va_arg(vl, int));
        va_end(vl);
    }

    return openat(dirfd, path, flags | O_LARGEFILE, mode);
}

/******  FILE OPS  ******/

inline int file_to_fd(const FILE* f){
    assert(f != nullptr);
    return *(reinterpret_cast<int*>(&f));
}

inline FILE* fd_to_file(const int fd){
    assert(fd >= 0);
    return reinterpret_cast<FILE*>(fd);
}

FILE* fopen(const char* path, const char* fmode) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path '{}' with mode '{}'", __func__, path, fmode);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            int flags = 0;
            std::string str_mode(fmode);
            if(str_mode == "r") {
                flags = O_RDONLY;
            } else if(str_mode == "r+") {
                flags = O_RDWR;
            } else if(str_mode == "w") {
                flags = (O_WRONLY | O_CREAT | O_TRUNC);
            } else {
                CTX->log()->error("{}() stream open flags NOT SUPPORTED: '{}'", __func__, str_mode);
                errno = ENOTSUP;
                return nullptr;
            }
            mode_t mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            auto fd = adafs_open(rel_path, mode, flags);
            if(fd == -1){
                return nullptr;
            } else {
                return fd_to_file(fd);
            }
        }
    }
    return (reinterpret_cast<decltype(&fopen)>(libc_fopen))(path, fmode);
}

FILE* fopen64(const char* path, const char* fmode) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path '{}' with mode '{}'", __func__, path, fmode);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return fopen(path, fmode);
        }
    }
    return (reinterpret_cast<decltype(&fopen64)>(libc_fopen64))(path, fmode);
}

size_t intcp_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    init_passthrough_if_needed();
    if(CTX->initialized() && (stream != nullptr)) {
        auto fd = file_to_fd(stream);
        if (CTX->file_map()->exist(fd)) {
            CTX->log()->trace("{}() called with fd {}", __func__, fd);
            auto adafs_fd = CTX->file_map()->get(fd);
            auto pos = adafs_fd->pos(); //retrieve the current offset
            auto ret = adafs_pread_ws(fd, ptr, size*nmemb, pos);
            if (ret > 0) {
                // Update offset in file descriptor in the file map
                adafs_fd->pos(pos + ret);
                return ret % size;
            }
            return ret;
        }
    }
    return (reinterpret_cast<decltype(&fread)>(libc_fread))(ptr, size, nmemb, stream);
}

size_t intcp_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    init_passthrough_if_needed();
    if(CTX->initialized() && (stream != nullptr)) {
        auto fd = file_to_fd(stream);
        if (CTX->file_map()->exist(fd)) {
            CTX->log()->trace("{}() called with fd {}", __func__, fd);
            auto adafs_fd = CTX->file_map()->get(fd);
            auto pos = adafs_fd->pos(); //retrieve the current offset
            auto ret = adafs_pwrite_ws(fd, ptr, size*nmemb, pos);
            if (ret > 0) {
                // Update offset in file descriptor in the file map
                adafs_fd->pos(pos + ret);
                return ret % size;
            }
            return ret;
        }
    }
    return (reinterpret_cast<decltype(&fwrite)>(libc_fwrite))(ptr, size, nmemb, stream);
}

int fclose(FILE *stream) {
    init_passthrough_if_needed();
    if(CTX->initialized() && (stream != nullptr)) {
        auto fd = file_to_fd(stream);
        if(CTX->file_map()->exist(fd)) {
            // No call to the daemon is required
            CTX->file_map()->remove(fd);
            return 0;
        }
    }
    return (reinterpret_cast<decltype(&fclose)>(libc_fclose))(stream);
}

int fileno(FILE *stream) {
    init_passthrough_if_needed();
    if(CTX->initialized() && (stream != nullptr)) {
        auto fd = file_to_fd(stream);
        if(CTX->file_map()->exist(fd)) {
            return fd;
        }
    }
    return (reinterpret_cast<decltype(&fileno)>(libc_fileno))(stream);
}

void clearerr(FILE *stream) {
    init_passthrough_if_needed();
    if(CTX->initialized() && (stream != nullptr)) {
        auto fd = file_to_fd(stream);
        if(CTX->file_map()->exist(fd)) {
            CTX->log()->error("{}() NOT SUPPORTED", __func__);
            errno = ENOTSUP;
            return;
        }
    }
    return (reinterpret_cast<decltype(&clearerr)>(libc_clearerr))(stream);
}

int feof(FILE *stream) {
    init_passthrough_if_needed();
    if(CTX->initialized() && (stream != nullptr)) {
        auto fd = file_to_fd(stream);
        if(CTX->file_map()->exist(fd)) {
            CTX->log()->error("{}() NOT SUPPORTED", __func__);
            errno = ENOTSUP;
            return -1;
        }
    }
    return (reinterpret_cast<decltype(&feof)>(libc_feof))(stream);
}

int ferror(FILE *stream) {
    init_passthrough_if_needed();
    if(CTX->initialized() && (stream != nullptr)) {
        auto fd = file_to_fd(stream);
        if(CTX->file_map()->exist(fd)) {
            CTX->log()->error("{}() NOT SUPPORTED", __func__);
            errno = ENOTSUP;
            return -1;
        }
    }
    return (reinterpret_cast<decltype(&ferror)>(libc_ferror))(stream);
}

/******  FILE OPS  ******/

#undef creat

int creat(const char* path, mode_t mode) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {} with mode {}", __func__, path, mode);
    }
    return open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

#undef creat64

int creat64(const char* path, mode_t mode) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {} with mode {}", __func__, path, mode);
    }
    return open(path, O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE, mode);
}

int mkdir(const char* path, mode_t mode) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {} with mode {}", __func__, path, mode);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_mk_node(rel_path, mode | S_IFDIR);
        }
    }
    return (reinterpret_cast<decltype(&mkdir)>(libc_mkdir))(path, mode);
}

int mkdirat(int dirfd, const char* path, mode_t mode) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {} with mode {} with dirfd {}", __func__, path, mode, dirfd);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            // not implemented
            CTX->log()->trace("{}() not implemented.", __func__);
            errno = EBUSY;
            return -1;
        }
    }
    return (reinterpret_cast<decltype(&mkdirat)>(libc_mkdirat))(dirfd, path, mode);
}


int unlink(const char* path) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_rm_node(rel_path);
        }
    }
    return (reinterpret_cast<decltype(&unlink)>(libc_unlink))(path);
}

int rmdir(const char* path) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_rmdir(rel_path);
        }
    }
    return (reinterpret_cast<decltype(&rmdir)>(libc_rmdir))(path);
}

int close(int fd) {
    init_passthrough_if_needed();
    if(CTX->initialized() && CTX->file_map()->exist(fd)) {
        // No call to the daemon is required
        CTX->file_map()->remove(fd);
        return 0;
    }
    return (reinterpret_cast<decltype(&close)>(libc_close))(fd);
}

int __close(int fd) {
    return close(fd);
}

int remove(const char* path) {
   return unlink(path);
}

int access(const char* path, int mask) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called path {} mask {}", __func__, path, mask);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_access(rel_path, mask);
        }
    }
    return (reinterpret_cast<decltype(&access)>(libc_access))(path, mask);
}

int faccessat(int dirfd, const char* path, int mode, int flags) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called path {} mode {} dirfd {} flags {}", __func__, path, mode, dirfd, flags);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            // not implemented
            CTX->log()->trace("{}() not implemented.", __func__);
            return -1;
        }
    }
    return (reinterpret_cast<decltype(&faccessat)>(libc_faccessat))(dirfd, path, mode, flags);
}


int stat(const char* path, struct stat* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_stat(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&stat)>(libc_stat))(path, buf);
}

int fstat(int fd, struct stat* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            auto path = CTX->file_map()->get(fd)->path();
            return adafs_stat(path, buf);
        }
    }
    return (reinterpret_cast<decltype(&fstat)>(libc_fstat))(fd, buf);
}

int lstat(const char* path, struct stat* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            CTX->log()->warn("{}() No symlinks are supported. Stats will always target the given path", __func__);
            return adafs_stat(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&lstat)>(libc_lstat))(path, buf);
}

int __xstat(int ver, const char* path, struct stat* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_stat(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__xstat)>(libc___xstat))(ver, path, buf);
}

int __xstat64(int ver, const char* path, struct stat64* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_stat64(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__xstat64)>(libc___xstat64))(ver, path, buf);
}

int __fxstat(int ver, int fd, struct stat* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            auto path = CTX->file_map()->get(fd)->path();
            return adafs_stat(path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__fxstat)>(libc___fxstat))(ver, fd, buf);
}

int __fxstatat(int ver, int dirfd, const char * path, struct stat * buf, int flags) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path '{}'", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_stat(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__fxstatat)>(libc___fxstatat))(ver, dirfd, path, buf, flags);
}

int __fxstatat64(int ver, int dirfd, const char * path, struct stat64 * buf, int flags) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path '{}'", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_stat64(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__fxstatat64)>(libc___fxstatat64))(ver, dirfd, path, buf, flags);

}

int __fxstat64(int ver, int fd, struct stat64* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            auto path = CTX->file_map()->get(fd)->path();
            return adafs_stat64(path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__fxstat64)>(libc___fxstat64))(ver, fd, buf);
}

int __lxstat(int ver, const char* path, struct stat* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_stat(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__lxstat)>(libc___lxstat))(ver, path, buf);
}

int __lxstat64(int ver, const char* path, struct stat64* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            return adafs_stat64(rel_path, buf);
        }
    }
    return (reinterpret_cast<decltype(&__lxstat64)>(libc___lxstat64))(ver, path, buf);
}

int statfs(const char* path, struct statfs* buf) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            // get information of the underlying fs.
            // Note, we explicitely call the real glibc statfs function to not intercept it again on the mountdir path
            struct statfs realfs{};
            auto ret = (reinterpret_cast<decltype(&statfs)>(libc_statfs))(CTX->mountdir().c_str(), &realfs);
            if (ret != 0)
                return ret;
            return adafs_statfs(rel_path, buf, realfs);
        }
    }
    return (reinterpret_cast<decltype(&statfs)>(libc_statfs))(path, buf);
}

int fstatfs(int fd, struct statfs* buf) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            auto adafs_fd = CTX->file_map()->get(fd);
            // get information of the underlying fs
            // Note, we explicitely call the real glibc statfs function to not intercept it again on the mountdir path
            struct statfs realfs{};
            auto ret = (reinterpret_cast<decltype(&statfs)>(libc_statfs))(CTX->mountdir().c_str(), &realfs);
            if (ret != 0)
                return ret;
            return adafs_statfs(adafs_fd->path(), buf, realfs);
        }
    }
    return (reinterpret_cast<decltype(&fstatfs)>(libc_fstatfs))(fd, buf);
}

int puts(const char* str) {
    return (reinterpret_cast<decltype(&puts)>(libc_puts))(str);
}

ssize_t write(int fd, const void* buf, size_t count) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            auto adafs_fd = CTX->file_map()->get(fd);
            auto pos = adafs_fd->pos(); // retrieve the current offset
            if (adafs_fd->get_flag(OpenFile_flags::append))
                adafs_lseek(adafs_fd, 0, SEEK_END);
            auto ret = adafs_pwrite_ws(fd, buf, count, pos);
            // Update offset in file descriptor in the file map
            if (ret > 0) {
                adafs_fd->pos(pos + count);
            }
            return ret;
        }
    }
    return (reinterpret_cast<decltype(&write)>(libc_write))(fd, buf, count);
}

ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            return adafs_pwrite_ws(fd, buf, count, offset);
        }
    }
    return (reinterpret_cast<decltype(&pwrite)>(libc_pwrite))(fd, buf, count, offset);
}

ssize_t pwrite64(int fd, const void* buf, size_t count, __off64_t offset) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            return adafs_pwrite_ws(fd, buf, count, offset);
        }
    }
    return (reinterpret_cast<decltype(&pwrite64)>(libc_pwrite64))(fd, buf, count, offset);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            auto adafs_fd = CTX->file_map()->get(fd);
            auto pos = adafs_fd->pos(); // retrieve the current offset
            ssize_t written = 0;
            ssize_t ret;
            for (int i = 0; i < iovcnt; ++i){
                auto buf = (iov+i)->iov_base;
                auto count = (iov+i)->iov_len;
                ret = adafs_pwrite_ws(fd, buf, count, pos);
                if(ret == -1) {
                    break;
                }
                written += ret;
                pos += ret;

                if(static_cast<size_t>(ret) < count){
                    break;
                }
            }

            if(written == 0){
                return -1;
            }
            adafs_fd->pos(pos);
            return written;
        }
    }
    return (reinterpret_cast<decltype(&writev)>(libc_writev))(fd, iov, iovcnt);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            CTX->log()->error("{}() NOT SUPPORTED", __func__);
            errno = ENOTSUP;
            return -1;
        }
    }
    return (reinterpret_cast<decltype(&readv)>(libc_readv))(fd, iov, iovcnt);
}

ssize_t read(int fd, void* buf, size_t count) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            auto adafs_fd = CTX->file_map()->get(fd);
            auto pos = adafs_fd->pos(); //retrieve the current offset
            auto ret = adafs_pread_ws(fd, buf, count, pos);
            // Update offset in file descriptor in the file map
            if (ret > 0) {
                adafs_fd->pos(pos + ret);
            }
            return ret;
        }
    }
    return (reinterpret_cast<decltype(&read)>(libc_read))(fd, buf, count);
}

ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            return adafs_pread_ws(fd, buf, count, offset);
        }
    }
    return (reinterpret_cast<decltype(&pread)>(libc_pread))(fd, buf, count, offset);
}

ssize_t pread64(int fd, void* buf, size_t count, __off64_t offset) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {}", __func__, fd);
        if (CTX->file_map()->exist(fd)) {
            return adafs_pread_ws(fd, buf, count, offset);
        }
    }
    return (reinterpret_cast<decltype(&pread64)>(libc_pread64))(fd, buf, count, offset);
}

off_t lseek(int fd, off_t offset, int whence) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {} with mode {}", __func__, fd, offset, whence);
        if (CTX->file_map()->exist(fd)) {
            auto off_ret = adafs_lseek(fd, static_cast<off64_t>(offset), whence);
            if (off_ret > std::numeric_limits<off_t>::max()) {
                errno = EOVERFLOW;
                return -1;
            } else {
                return off_ret;
            }
        }
    }
   return (reinterpret_cast<decltype(&lseek)>(libc_lseek))(fd, offset, whence);
}

#undef lseek64
off64_t lseek64(int fd, off64_t offset, int whence) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {} with mode {}", __func__, fd, offset, whence);
        if (CTX->file_map()->exist(fd)) {
            return adafs_lseek(fd, offset, whence);
        }
    }
    return (reinterpret_cast<decltype(&lseek64)>(libc_lseek64))(fd, offset, whence);
}

int fsync(int fd) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {} path {}", __func__, fd, CTX->file_map()->get(fd)->path());
        if (CTX->file_map()->exist(fd)) {
            return 0; // This is a noop for us atm. fsync is called implicitly because each chunk is closed after access
        }
    }
    return (reinterpret_cast<decltype(&fsync)>(libc_fsync))(fd);
}

int fdatasync(int fd) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with fd {} path {}", __func__, fd, CTX->file_map()->get(fd)->path());
        if (CTX->file_map()->exist(fd)) {
            return 0; // This is a noop for us atm. fsync is called implicitly because each chunk is closed after access
        }
    }
    return (reinterpret_cast<decltype(&fdatasync)>(libc_fdatasync))(fd);
}

int truncate(const char* path, off_t length) __THROW {
    init_passthrough_if_needed();
    return (reinterpret_cast<decltype(&truncate)>(libc_truncate))(path, length);
}

int ftruncate(int fd, off_t length) __THROW {
    init_passthrough_if_needed();
    return (reinterpret_cast<decltype(&ftruncate)>(libc_ftruncate))(fd, length);
}

int dup(int oldfd) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with oldfd {}", __func__, oldfd);
        if (CTX->file_map()->exist(oldfd)) {
            return adafs_dup(oldfd);
        }
    }
    return (reinterpret_cast<decltype(&dup)>(libc_dup))(oldfd);
}

int dup2(int oldfd, int newfd) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with oldfd {} newfd {}", __func__, oldfd, newfd);
        if (CTX->file_map()->exist(oldfd)) {
            return adafs_dup2(oldfd, newfd);
        }
    }
    return (reinterpret_cast<decltype(&dup2)>(libc_dup2))(oldfd, newfd);
}

int dup3(int oldfd, int newfd, int flags) __THROW {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        if (CTX->file_map()->exist(oldfd)) {
            // TODO implement O_CLOEXEC flag first which is used with fcntl(2)
            // It is in glibc since kernel 2.9. So maybe not that important :)
            CTX->log()->error("{}() Not implemented.", __func__);
            errno = EBUSY;
            return -1;
        }
    }
    return (reinterpret_cast<decltype(&dup3)>(libc_dup3))(oldfd, newfd, flags);
}

/* Directories related calls */

inline int dirp_to_fd(const DIR* dirp){
    assert(dirp != nullptr);
    return *(reinterpret_cast<int*>(&dirp));
}

inline DIR* fd_to_dirp(const int fd){
    assert(fd >= 0);
    return reinterpret_cast<DIR*>(fd);
}

DIR* opendir(const char* path){
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        CTX->log()->trace("{}() called with path {}", __func__, path);
        std::string rel_path(path);
        if (CTX->relativize_path(rel_path)) {
            auto fd = adafs_opendir(rel_path);
            if(fd < 0){
                return nullptr;
            }
            return fd_to_dirp(fd);
        }
    }
    return (reinterpret_cast<decltype(&opendir)>(libc_opendir))(path);
}

struct dirent* intcp_readdir(DIR* dirp){
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        if(dirp == nullptr){
            errno = EINVAL;
            return nullptr;
        }
        auto fd = dirp_to_fd(dirp);
        if(CTX->file_map()->exist(fd)) {
            return adafs_readdir(fd);
        }
    }
    return (reinterpret_cast<decltype(&readdir)>(libc_readdir))(dirp);
}

int intcp_closedir(DIR* dirp) {
    init_passthrough_if_needed();
    if(CTX->initialized()) {
        if(dirp == nullptr){
            errno = EINVAL;
            return -1;
        }
        auto fd = dirp_to_fd(dirp);
        if (CTX->file_map()->exist(fd)) {
            // No call to the daemon is required
            CTX->file_map()->remove(fd);
            return 0;
        }
    }
    return (reinterpret_cast<decltype(&closedir)>(libc_closedir))(dirp);
}

int chdir(const char* path){
    init_passthrough_if_needed();
    CTX->log()->trace("{}() called with path {}", __func__, path);
    std::string rel_path(path);
    if (CTX->relativize_path(rel_path)) {
        CTX->log()->error("Attempt to chdir into adafs namespace: NOT SUPPORTED", __func__, path);
        errno = ENOTSUP;
        return -1;
    }
    return (reinterpret_cast<decltype(&chdir)>(libc_chdir))(path);
}

char *realpath(const char *path, char *resolved_path) {
    init_passthrough_if_needed();
    CTX->log()->trace("{}() called with path {}", __func__, path);
    std::string rel_path(path);
    if (CTX->relativize_path(rel_path)) {
        if(resolved_path != nullptr) {
            CTX->log()->error("{}() use of user level buffer not supported", __func__);
            errno = ENOTSUP;
            return nullptr;
        }
        auto absolute_path = CTX->mountdir() + rel_path;
        auto ret_ptr = static_cast<char*>(malloc(absolute_path.size() +  1));
        if(ret_ptr == nullptr){
            CTX->log()->error("{}() failed to allocate buffer for called with path {}", __func__, path);
            errno = ENOMEM;
            return nullptr;
        }
        strcpy(ret_ptr, absolute_path.c_str());
        return ret_ptr;
    }
    return (reinterpret_cast<decltype(&realpath)>(libc_realpath))(path, resolved_path);
}
