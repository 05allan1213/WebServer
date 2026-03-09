#include "base/PerfConfig.h"
#include "log/Log.h"

PerfConfig::PerfConfig(const YAML::Node &node)
    : node_(node)
{
    int datasetSize = getMemoryDatasetSize();
    int batchDefaultSize = getBatchDefaultSize();
    int statsPollIntervalMs = getStatsPollIntervalMs();
    validateConfig(datasetSize, batchDefaultSize, statsPollIntervalMs);
}

int PerfConfig::getMemoryDatasetSize() const
{
    if (node_ && node_["memory_dataset_size"])
    {
        return node_["memory_dataset_size"].as<int>();
    }
    return 512;
}

int PerfConfig::getBatchDefaultSize() const
{
    if (node_ && node_["batch_default_size"])
    {
        return node_["batch_default_size"].as<int>();
    }
    return 16;
}

bool PerfConfig::isDbModeEnabled() const
{
    if (node_ && node_["enable_db_mode"])
    {
        return node_["enable_db_mode"].as<bool>();
    }
    return true;
}

std::string PerfConfig::getStaticPerfDir() const
{
    if (node_ && node_["static_perf_dir"])
    {
        return node_["static_perf_dir"].as<std::string>();
    }
    return "web_static/perf";
}

int PerfConfig::getStatsPollIntervalMs() const
{
    if (node_ && node_["stats_poll_interval_ms"])
    {
        return node_["stats_poll_interval_ms"].as<int>();
    }
    return 1000;
}

void PerfConfig::validateConfig(int datasetSize, int batchDefaultSize, int statsPollIntervalMs) const
{
    if (datasetSize <= 0)
    {
        throw std::invalid_argument("perf.memory_dataset_size 必须大于 0");
    }
    if (batchDefaultSize <= 0)
    {
        throw std::invalid_argument("perf.batch_default_size 必须大于 0");
    }
    if (statsPollIntervalMs < 200)
    {
        throw std::invalid_argument("perf.stats_poll_interval_ms 不能小于 200");
    }
}
