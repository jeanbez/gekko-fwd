
#include <global/rpc/rpc_utils.hpp>

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