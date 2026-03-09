#pragma once

#include <yaml-cpp/yaml.h>
#include <string>

/**
 * @brief 性能展示平台配置
 *
 * 集中管理 perf 子系统的最小配置项，保证 memory/db 双模式、
 * 静态性能文件目录和前端轮询间隔都能通过统一入口读取。
 */
class PerfConfig
{
public:
    explicit PerfConfig(const YAML::Node &node);

    int getMemoryDatasetSize() const;
    int getBatchDefaultSize() const;
    bool isDbModeEnabled() const;
    std::string getStaticPerfDir() const;
    int getStatsPollIntervalMs() const;

private:
    void validateConfig(int datasetSize, int batchDefaultSize, int statsPollIntervalMs) const;

    YAML::Node node_;
};
