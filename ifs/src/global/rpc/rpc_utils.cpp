
#include <global/rpc/rpc_utils.hpp>
#include <unistd.h>

using namespace std;

/**
 * converts std bool to mercury bool
 * @param state
 * @return
 */
hg_bool_t bool_to_merc_bool(const bool state) {
    return state ? static_cast<hg_bool_t>(HG_TRUE) : static_cast<hg_bool_t>(HG_FALSE);
}


/**
 * Returns the machine's hostname
 * @return
 */
std::string get_my_hostname(bool short_hostname) {
    char hostname[1024];
    auto ret = gethostname(hostname, 1024);
    if (ret == 0) {
        std::string hostname_s(hostname);
        if (!short_hostname)
            return hostname_s;
        // get short hostname
        auto pos = hostname_s.find("."s);
        if (pos != std::string::npos)
            hostname_s = hostname_s.substr(0, pos);
        return hostname_s;
    } else
        return ""s;
}

/**
 * checks if a Mercury handle's address is shared memory
 * @param mid
 * @param addr
 * @return bool
 */
bool is_handle_sm(margo_instance_id mid, const hg_addr_t& addr) {
    hg_size_t size = 128;
    char addr_cstr[128];
    if (margo_addr_to_string(mid, addr_cstr, &size, addr) != HG_SUCCESS)
        return false;
    string addr_str(addr_cstr);
    return addr_str.substr(0, 5) == "na+sm";
}