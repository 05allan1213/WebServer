#include "http/perf/PerfPlatform.h"
#include "base/PerfConfig.h"
#include "http/perf/PerfMetrics.h"

PerfPlatform &PerfPlatform::instance()
{
    static PerfPlatform platform;
    return platform;
}

void PerfPlatform::initialize(const std::shared_ptr<PerfConfig> &config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!memoryRepository_)
    {
        memoryRepository_ = std::make_shared<MemoryPerfRepository>();
    }
    if (!dbRepository_)
    {
        dbRepository_ = std::make_shared<MySQLPerfRepository>();
    }
    applyConfigLocked(config);
}

void PerfPlatform::refresh(const std::shared_ptr<PerfConfig> &config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    applyConfigLocked(config);
}

std::shared_ptr<PerfConfig> PerfPlatform::config() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::shared_ptr<MemoryPerfRepository> PerfPlatform::memoryRepository() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return memoryRepository_;
}

std::shared_ptr<MySQLPerfRepository> PerfPlatform::dbRepository() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return dbRepository_;
}

std::string PerfPlatform::staticPerfDir() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return staticPerfDir_;
}

std::shared_ptr<PerfRepository> PerfPlatform::selectRepository(const std::string &mode,
                                                               bool *usedDb,
                                                               std::string *error) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (usedDb)
    {
        *usedDb = false;
    }
    if (error)
    {
        error->clear();
    }

    if (mode == "db")
    {
        if (!config_ || !config_->isDbModeEnabled())
        {
            if (error)
            {
                *error = "配置未启用 db 模式";
            }
            return nullptr;
        }
        if (usedDb)
        {
            *usedDb = true;
        }
        return dbRepository_;
    }
    return memoryRepository_;
}

void PerfPlatform::applyConfigLocked(const std::shared_ptr<PerfConfig> &config)
{
    config_ = config;
    if (!memoryRepository_)
    {
        memoryRepository_ = std::make_shared<MemoryPerfRepository>();
    }
    if (!dbRepository_)
    {
        dbRepository_ = std::make_shared<MySQLPerfRepository>();
    }

    if (config_)
    {
        memoryRepository_->seed(config_->getMemoryDatasetSize());
        staticPerfDir_ = config_->getStaticPerfDir();
        PerfMetrics::instance().setDbModeEnabled(config_->isDbModeEnabled());
        PerfMetrics::instance().setMemoryDatasetSize(config_->getMemoryDatasetSize());
    }
    else
    {
        memoryRepository_->seed(512);
        staticPerfDir_ = "web_static/perf";
        PerfMetrics::instance().setDbModeEnabled(true);
        PerfMetrics::instance().setMemoryDatasetSize(512);
    }

    PerfMetrics::instance().setPerfDbAvailable(dbRepository_->available());
}
