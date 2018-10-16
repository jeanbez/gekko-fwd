
#ifndef LFS_RPC_DATA_HPP
#define LFS_RPC_DATA_HPP

#include <daemon/adafs_daemon.hpp>

class RPCData {

private:
    RPCData() {}

    // Margo IDs. They can also be used to retrieve the Mercury classes and contexts that were created at init time
    margo_instance_id server_rpc_mid_;
    margo_instance_id server_ipc_mid_;

    // Argobots I/O pools and execution streams
    ABT_pool io_pool_;
    std::vector<ABT_xstream> io_streams_;
    std::string self_addr_str_;

public:

    static RPCData* getInstance() {
        static RPCData instance;
        return &instance;
    }

    RPCData(RPCData const&) = delete;

    void operator=(RPCData const&) = delete;

    // Getter/Setter

    margo_instance* server_rpc_mid();

    void server_rpc_mid(margo_instance* server_rpc_mid);

    margo_instance* server_ipc_mid();

    void server_ipc_mid(margo_instance* server_ipc_mid);

    ABT_pool io_pool() const;

    void io_pool(ABT_pool io_pool);

    std::vector<ABT_xstream>& io_streams();

    void io_streams(const std::vector<ABT_xstream>& io_streams);

    const std::string& self_addr_str() const;

    void self_addr_str(const std::string& addr_str);


};


#endif //LFS_RPC_DATA_HPP
