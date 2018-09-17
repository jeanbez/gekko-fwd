#include <preload/preload_context.hpp>

#include <preload/open_file_map.hpp>
#include <preload/resolve.hpp>
#include <global/path_util.hpp>
#include <cassert>


PreloadContext::PreloadContext():
    ofm_(std::make_shared<OpenFileMap>()),
    fs_conf_(std::make_shared<FsConfig>())
{}

void PreloadContext::log(std::shared_ptr<spdlog::logger> logger) {
    log_ = logger;
}

std::shared_ptr<spdlog::logger> PreloadContext::log() const {
    return log_;
}

void PreloadContext::mountdir(const std::string& path) {
    assert(is_absolute_path(path));
    assert(!has_trailing_slash(path));
    mountdir_components_ = split_path(path);
    mountdir_ = path;
}

const std::string& PreloadContext::mountdir() const {
    return mountdir_;
}

const std::vector<std::string>& PreloadContext::mountdir_components() const {
    return mountdir_components_;
}

void PreloadContext::cwd(const std::string& path) {
    log_->debug("Setting CWD to '{}'", path);
    cwd_ = path;
}

const std::string& PreloadContext::cwd() const {
    return cwd_;
}

bool PreloadContext::relativize_path(const char * raw_path, std::string& relative_path) const {
    // Relativize path should be called only after the library constructor has been executed
    assert(initialized_);
    // If we run the constructor we also already setup the mountdir
    assert(!mountdir_.empty());

    if(raw_path == nullptr || raw_path[0] == '\0') {
        return false;
    }

    std::string path;

    if(raw_path[0] != PSP) {
        /* Path is not absolute, we need to prepend CWD;
         * First reserve enough space to minimize memory copy
         */
        path = prepend_path(cwd_, raw_path);
    } else {
        path = raw_path;
    }
    return resolve_path(path, relative_path);
}

const std::shared_ptr<OpenFileMap>& PreloadContext::file_map() const {
    return ofm_;
}

void PreloadContext::distributor(std::shared_ptr<Distributor> d) {
    distributor_ = d;
}

std::shared_ptr<Distributor> PreloadContext::distributor() const {
    return distributor_;
}

const std::shared_ptr<FsConfig>& PreloadContext::fs_conf() const {
    return fs_conf_;
}

void PreloadContext::initialized(const bool& flag) {
    initialized_ = flag;
}

bool PreloadContext::initialized() const {
    return initialized_;
}
