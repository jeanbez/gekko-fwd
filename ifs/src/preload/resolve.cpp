#include <vector>
#include <string>
#include <sys/stat.h>
#include <cassert>
#include <libsyscall_intercept_hook_point.h>

#include "global/path_util.hpp"
#include "preload/preload.hpp"



bool resolve_path (const std::string& path, std::string& resolved) {
    CTX->log()->debug("{}() path: '{}'", __func__, path);

    struct stat st;
    const std::vector<std::string>& mnt_components = CTX->mountdir_components();
    unsigned int mnt_matched = 0; // matched number of component in mountdir
    std::string::size_type comp_size = 0; // size of current component
    std::string::size_type start = 0; // start index of curr component
    std::string::size_type end = 0; // end index of curr component (last processed PSP)
    std::string::size_type last_slash_pos = 0; // index of last slash in resolved path
    std::string component;
    resolved.clear();
    resolved.reserve(path.size());

    //Process first slash
    assert(is_absolute_path(path));

    while (++end < path.size()) {
        start = end;

        /* Skip sequence of multiple path-separators. */
        while(start < path.size() && path[start] == PSP) {
            ++start;
        }

        // Find next component
        end = path.find(PSP, start);
        if(end == std::string::npos) {
            end = path.size();
        }
        comp_size = end - start;

        if (comp_size == 0) {
            // component is empty (this must be the last component)
            break;
        }
        if (comp_size == 1 && path.at(start) == '.') {
            // component is '.', we skip it
            continue;
        }
        if (comp_size == 2 && path.at(start) == '.' && path.at(start+1) == '.') {
            // component is '..' we need to rollback resolved path
            if(resolved.size() > 0) {
                resolved.erase(last_slash_pos);
            }
            if(mnt_matched > 0) {
                --mnt_matched;
            }
            continue;
        }

        // add `/<component>` to the reresolved path
        resolved.push_back(PSP);
        last_slash_pos = resolved.size() - 1;
        resolved.append(path, start, comp_size);

        if (mnt_matched < mnt_components.size()) {
            // Outside GekkoFS
            if (path.compare(start, comp_size, mnt_components.at(mnt_matched)) == 0) {
                ++mnt_matched;
            }
            if (lstat(resolved.c_str(), &st) < 0) {
                resolved.append(path, end, std::string::npos);
                return false;
            }
            if (S_ISLNK(st.st_mode)) {
               CTX->log()->error("{}() encountered link: {}", __func__, resolved);
               throw std::runtime_error("Readlink encoutered: '" + resolved + "'");
            } else if ((!S_ISDIR(st.st_mode)) && (end != path.size())) {
               resolved.append(path, end, std::string::npos);
               return false;
            }
        } else {
            // Inside GekkoFS
            ++mnt_matched;
        }
    }
    if (mnt_matched >= mnt_components.size()) {
        resolved.erase(1, CTX->mountdir().size());
        CTX->log()->debug("{}() internal: '{}'", __func__, resolved);
        return true;
    }

    if (resolved.size() == 0) {
        resolved.push_back(PSP);
    }
    CTX->log()->debug("{}() external: '{}'", __func__, resolved);
    return false;
}

std::string get_sys_cwd() {
    char temp[PATH_MAX_LEN];
    if (long ret = syscall_no_intercept(SYS_getcwd, temp, PATH_MAX_LEN) < 0) {
        throw std::system_error(syscall_error_code(ret),
                                std::system_category(),
                                "Failed to retrieve current working directory");
    }
    // getcwd could return "(unreachable)<PATH>" in some cases
    if(temp[0] != PSP) {
        throw std::runtime_error(
                "Current working directory is unreachable");
    }
    return {temp};
}

void set_sys_cwd(const std::string& path) {
    CTX->log()->debug("{}() to '{}'", __func__, path);
    if (long ret = syscall_no_intercept(SYS_chdir, path.c_str())) {
        CTX->log()->error("{}() failed to set system current working directory: {}",
                __func__, std::strerror(syscall_error_code(ret)));
        throw std::system_error(syscall_error_code(ret),
                                std::system_category(),
                                "Failed to set system current working directory");
    }
}

void set_env_cwd(const std::string& path) {
    CTX->log()->debug("{}() to '{}'", __func__, path);
    if(setenv("ADAFS_CWD", path.c_str(), 1)) {
        CTX->log()->error("{}() failed to set environment current working directory: {}",
                __func__, std::strerror(errno));
        throw std::system_error(errno,
                                std::system_category(),
                                "Failed to set environment current working directory");
    }
}

void unset_env_cwd() {
    CTX->log()->debug("{}()", __func__);
    if(unsetenv("ADAFS_CWD")) {
        CTX->log()->error("{}() failed to unset environment current working directory: {}",
                __func__, std::strerror(errno));
        throw std::system_error(errno,
                                std::system_category(),
                                "Failed to unset environment current working directory");
    }
}

void init_cwd() {
    const char* env_cwd = std::getenv("ADAFS_CWD");
    if (env_cwd != nullptr) {
        CTX->cwd(env_cwd);
    } else {
        CTX->cwd(get_sys_cwd());
    }
}

void set_cwd(const std::string& path, bool internal) {
    if(internal) {
        set_sys_cwd(CTX->mountdir());
        set_env_cwd(path);
    } else {
        set_sys_cwd(path);
        unset_env_cwd();
    }
    CTX->cwd(path);
}
