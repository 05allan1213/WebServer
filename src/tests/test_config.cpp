#include "base/Config.h"
#include <iostream>

int main()
{
    Config::getInstance().load("configs/config.yml");
    const auto &config = Config::getInstance();

    std::cout << "log.basename: " << config.getLogBasename() << std::endl;
    std::cout << "log.roll_size: " << config.getLogRollSize() << std::endl;
    std::cout << "log.flush_interval: " << config.getLogFlushInterval() << std::endl;
    std::cout << "log.roll_mode: " << config.getLogRollMode() << std::endl;
    std::cout << "log.enable_file: " << config.getLogEnableFile() << std::endl;
    std::cout << "log.file_level: " << config.getLogFileLevel() << std::endl;
    std::cout << "log.console_level: " << config.getLogConsoleLevel() << std::endl;
    std::cout << "network.ip: " << config.getNetworkIp() << std::endl;
    std::cout << "network.port: " << config.getNetworkPort() << std::endl;
    std::cout << "network.thread_num: " << config.getNetworkThreadNum() << std::endl;
    return 0;
}