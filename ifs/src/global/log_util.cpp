#include "global/log_util.hpp"
#include <exception>
#include <vector>
#include <list>


spdlog::level::level_enum get_spdlog_level(const std::string& level_str) {
    char* parse_end;
    unsigned long long uint_level = strtoul(level_str.c_str(), &parse_end, 10);
    if( parse_end != (level_str.c_str() + level_str.size())) {
        throw std::runtime_error("Error: log level has wrong format: '" + level_str + "'");
    }
    return get_spdlog_level(uint_level);
}

spdlog::level::level_enum get_spdlog_level(unsigned int level) {
    switch(level) {
        case 0:
            return spdlog::level::off;
        case 1:
            return spdlog::level::critical;
        case 2:
            return spdlog::level::err;
        case 3:
            return spdlog::level::warn;
        case 4:
            return spdlog::level::info;
        case 5:
            return spdlog::level::debug;
        default:
            return spdlog::level::trace;
    }
}

void setup_loggers(const std::vector<std::string>& loggers_name,
        spdlog::level::level_enum level, const std::string& path) {

        /* Create common sink */
        auto file_sink = std::make_shared<spdlog::sinks::simple_file_sink_mt>(path);

        /* Create and configure loggers */
        auto loggers = std::list<std::shared_ptr<spdlog::logger>>();
        for(const auto& name: loggers_name){
            auto logger = std::make_shared<spdlog::logger>(name, file_sink);
            logger->flush_on(spdlog::level::trace);
            loggers.push_back(logger);
        }

        /* register loggers */
        for(const auto& logger: loggers){
            spdlog::register_logger(logger);
        }

        // set logger format
        spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%f] %P [%L][%n] %v");

        spdlog::set_level(level);
}
