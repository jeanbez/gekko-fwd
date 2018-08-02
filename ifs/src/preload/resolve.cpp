#include <vector>
#include <string>
#include <sys/stat.h>
#include <cassert>

#include "global/path_util.hpp"
#include "preload/passthrough.hpp"
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
        while(path.at(start) == PSP) {
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
            if (LIBC_FUNC(__xstat, _STAT_VER, resolved.c_str(), &st) < 0) {
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
    CTX->log()->debug("{}() external: '{}'", __func__, resolved);
    return false;
}

std::string get_sys_cwd() {
    char temp[PATH_MAX_LEN];
    if(LIBC_FUNC(getcwd, temp, PATH_MAX_LEN) == NULL) {
        throw std::system_error(errno,
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
