#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct PerfItem
{
    long long id = 0;
    std::string name;
    std::string category;
    double score = 0.0;
    std::string source;
    std::string createdAt;
};

struct PerfWriteItem
{
    std::string name;
    std::string category;
    double score = 0.0;
};

class PerfRepository
{
public:
    virtual ~PerfRepository() = default;

    virtual std::string modeName() const = 0;
    virtual bool available() const = 0;
    virtual std::vector<PerfItem> listItems(size_t limit, std::string *error) = 0;
    virtual bool batchInsert(const std::vector<PerfWriteItem> &items, size_t *inserted, std::string *error) = 0;
};

class MemoryPerfRepository : public PerfRepository
{
public:
    MemoryPerfRepository();

    void seed(size_t count);

    std::string modeName() const override { return "memory"; }
    bool available() const override { return true; }
    std::vector<PerfItem> listItems(size_t limit, std::string *error) override;
    bool batchInsert(const std::vector<PerfWriteItem> &items, size_t *inserted, std::string *error) override;

private:
    mutable std::mutex mutex_;
    std::vector<PerfItem> items_;
    long long nextId_ = 1;
};

class MySQLPerfRepository : public PerfRepository
{
public:
    std::string modeName() const override { return "db"; }
    bool available() const override;
    std::vector<PerfItem> listItems(size_t limit, std::string *error) override;
    bool batchInsert(const std::vector<PerfWriteItem> &items, size_t *inserted, std::string *error) override;
};
