#include "http/handlers/StatsHandler.h"
#include "base/Buffer.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void statsHandler(const HttpRequest &req, HttpResponse *resp)
{
    json stats;
    stats["buffer"]["active_count"] = Buffer::getActiveBuffers();
    stats["buffer"]["pool_memory_bytes"] = Buffer::getPoolMemory();
    stats["buffer"]["heap_memory_bytes"] = Buffer::getHeapMemory();
    stats["buffer"]["resize_count"] = Buffer::getResizeCount();
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(stats.dump(4));
}
