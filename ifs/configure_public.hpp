
/**
 * Attention: This whole configfile is not in a final form! This will eventually be a plaintext config file.
 */

#ifndef FS_CONFIGURE_PUBLIC_H
#define FS_CONFIGURE_PUBLIC_H

// To enabled logging for daemon
//#define LOG_INFO
//#define LOG_DEBUG
#define LOG_TRACE
#define LOG_DAEMON_PATH "/tmp/adafs_daemon.log"

// Enable logging for preload
//#define LOG_PRELOAD_INFO
//#define LOG_PRELOAD_DEBUG
#define LOG_PRELOAD_TRACE
#define LOG_PRELOAD_PATH "/tmp/adafs_preload.log"

// Set a hostname suffix when a connection is built. E.g., "-ib" to use Infiniband
#define HOSTNAME_SUFFIX ""
//#define MARGODIAG // enables diagnostics of margo (printed after shutting down


#endif //FS_CONFIGURE_PUBLIC_H
