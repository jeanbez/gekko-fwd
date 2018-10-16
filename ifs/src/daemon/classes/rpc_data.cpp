
#include <daemon/classes/rpc_data.hpp>

using namespace std;

// Getter/Setter

margo_instance* RPCData::server_rpc_mid() {
    return server_rpc_mid_;
}

void RPCData::server_rpc_mid(margo_instance* server_rpc_mid) {
    RPCData::server_rpc_mid_ = server_rpc_mid;
}

margo_instance* RPCData::server_ipc_mid() {
    return server_ipc_mid_;
}

void RPCData::server_ipc_mid(margo_instance* server_ipc_mid) {
    RPCData::server_ipc_mid_ = server_ipc_mid;
}

ABT_pool RPCData::io_pool() const {
    return io_pool_;
}

void RPCData::io_pool(ABT_pool io_pool) {
    RPCData::io_pool_ = io_pool;
}

vector<ABT_xstream>& RPCData::io_streams() {
    return io_streams_;
}

void RPCData::io_streams(const vector<ABT_xstream>& io_streams) {
    RPCData::io_streams_ = io_streams;
}

const std::string& RPCData::self_addr_str() const {
    return self_addr_str_;
}

void RPCData::self_addr_str(const std::string& addr_str) {
    self_addr_str_ = addr_str;
}
