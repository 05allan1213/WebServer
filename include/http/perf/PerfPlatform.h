#pragma once

#include "http/perf/PerfRepository.h"
#include <memory>
#include <mutex>
#include <string>

class PerfConfig;

/**
 * @brief perf 子系统运行时入口
 *
 * 管理配置、memory/db 双仓储以及前端控制台需要的静态资源目录信息。
 */
class PerfPlatform
{
public:
    static PerfPlatform &instance();

    void initialize(const std::shared_ptr<PerfConfig> &config);
    void refresh(const std::shared_ptr<PerfConfig> &config);

    std::shared_ptr<PerfConfig> config() const;
    std::shared_ptr<MemoryPerfRepository> memoryRepository() const;
    std::shared_ptr<MySQLPerfRepository> dbRepository() const;

    std::shared_ptr<PerfRepository> selectRepository(const std::string &mode,
                                                     bool *usedDb,
                                                     std::string *error) const;

    std::string staticPerfDir() const;

private:
    PerfPlatform() = default;

    void applyConfigLocked(const std::shared_ptr<PerfConfig> &config);

    std::shared_ptr<PerfConfig> config_;
    std::shared_ptr<MemoryPerfRepository> memoryRepository_;
    std::shared_ptr<MySQLPerfRepository> dbRepository_;
    std::string staticPerfDir_ = "web_static/perf";
    mutable std::mutex mutex_;
};
