#include "StaticFileHandler.h"
#include <fstream>
#include <sys/stat.h>
#include <unordered_map>
#include "log/Log.h"

// 零拷贝阈值，例如 64KB
const off_t ZERO_COPY_THRESHOLD = 64 * 1024;

// 获取文件扩展名对应的 MIME-Type
static std::string getMimeType(const std::string &path)
{
    static std::unordered_map<std::string, std::string> mimeTypes = {
        {".html", "text/html"}, {".htm", "text/html"}, {".css", "text/css"}, {".js", "application/javascript"}, {".json", "application/json"}, {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".gif", "image/gif"}, {".svg", "image/svg+xml"}, {".ico", "image/x-icon"}, {".txt", "text/plain"}, {".pdf", "application/pdf"}, {".zip", "application/zip"}, {".rar", "application/x-rar-compressed"}};
    auto pos = path.rfind('.');
    if (pos != std::string::npos)
    {
        std::string ext = path.substr(pos);
        auto it = mimeTypes.find(ext);
        if (it != mimeTypes.end())
            return it->second;
    }
    return "application/octet-stream";
}

// 读取文件内容，如果文件不存在则返回默认内容
static std::string readFileOrDefault(const std::string &path, const std::string &def)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return def;
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

/**
 * @brief 处理静态文件请求
 * @param req   HTTP请求对象
 * @param resp  HTTP响应对象
 * @param baseDir 静态资源根目录，默认 web_static
 * @return true 表示已处理(无论成功与否)，false 表示未命中静态资源
 */
bool StaticFileHandler::handle(const HttpRequest &req, HttpResponse *resp, const std::string &baseDir)
{
    // 1. 路径映射，将URL路径映射到本地文件
    std::string urlPath = req.getPath();
    if (urlPath == "/")
        urlPath = "/index.html"; // 默认首页
    std::string filePath = baseDir + urlPath;

    DLOG_INFO << "[StaticFileHandler] 处理静态资源请求: " << filePath;

    struct stat st;
    // 2. 检查文件是否存在
    if (stat(filePath.c_str(), &st) != 0)
    {
        DLOG_WARN << "[StaticFileHandler] 文件不存在: " << filePath;
        std::string notFoundPath = baseDir + "/404.html";
        std::string body = readFileOrDefault(notFoundPath, "<html><body><h1>404 Not Found</h1></body></html>");
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("text/html");
        resp->setBody(body);
        return true;
    }
    // 3. 检查文件权限
    if (!(st.st_mode & S_IROTH))
    {
        DLOG_WARN << "[StaticFileHandler] 文件无权限访问: " << filePath;
        std::string forbiddenPath = baseDir + "/403.html";
        std::string body = readFileOrDefault(forbiddenPath, "<html><body><h1>403 Forbidden</h1></body></html>");
        resp->setStatusCode(HttpResponse::k403Forbidden);
        resp->setStatusMessage("Forbidden");
        resp->setContentType("text/html");
        resp->setBody(body);
        return true;
    }
    // 4. 检查请求是否合法(如方法、协议版本)
    if (req.getMethod() == HttpRequest::Method::kInvalid || req.getVersion() == HttpRequest::Version::kUnknown)
    {
        DLOG_WARN << "[StaticFileHandler] 非法请求: method=" << static_cast<int>(req.getMethod()) << ", version=" << static_cast<int>(req.getVersion());
        std::string badRequestPath = baseDir + "/400.html";
        std::string body = readFileOrDefault(badRequestPath, "<html><body><h1>400 Bad Request</h1></body></html>");
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setContentType("text/html");
        resp->setBody(body);
        return true;
    }
    // 5. 零拷贝判断
    if (st.st_size > ZERO_COPY_THRESHOLD)
    {
        DLOG_INFO << "[StaticFileHandler] 文件 " << filePath << " 大小超过阈值，使用零拷贝";
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType(getMimeType(filePath));
        resp->setContentLength(st.st_size);
        resp->setFilePath(filePath); // <-- 关键：设置文件路径，而不是body
        return true;
    }
    // 6. 打开文件，读取内容
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs)
    {
        DLOG_ERROR << "[StaticFileHandler] 文件打开失败: " << filePath;
        std::string errorPath = baseDir + "/500.html";
        std::string body = readFileOrDefault(errorPath, "<html><body><h1>500 Internal Server Error</h1></body></html>");
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setContentType("text/html");
        resp->setBody(body);
        return true;
    }
    // 7. 读取文件内容到body
    std::string body((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::string mime = getMimeType(filePath);
    DLOG_INFO << "[StaticFileHandler] 成功返回文件: " << filePath << ", MIME: " << mime << ", 大小: " << body.size();
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType(mime);
    resp->setBody(body); // <-- 读取文件内容到 body
    // 注意：传统方式不再需要手动设置 Content-Length，appendToBuffer 会自动计算
    return true;
}