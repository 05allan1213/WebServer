#include "StaticFileHandler.h"
#include <fstream>
#include <sys/stat.h>
#include <unordered_map>
#include <sstream>
#include <climits>
#include "log/Log.h"

/**
 * @brief 获取文件扩展名对应的MIME-Type
 * @param path 文件路径
 * @return MIME-Type字符串
 */
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

/**
 * @brief 读取文件内容，如果文件不存在则返回默认内容
 * @param path 文件路径
 * @param def 默认内容
 * @return 文件内容或默认内容
 */
static std::string readFileOrDefault(const std::string &path, const std::string &def)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return def;
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

/**
 * @brief 处理静态文件请求
 * @param req HTTP请求对象
 * @param resp HTTP响应对象
 * @param baseDir 静态资源根目录，默认web_static
 * @return true 表示已处理（无论成功与否），false 表示未命中静态资源
 *
 * 处理流程：
 * 1. 检查请求方法是否合法（只允许GET和HEAD）
 * 2. 将URL路径映射到本地文件
 * 3. 检查文件是否存在，不存在返回404
 * 4. 检查文件权限，无权限返回403
 * 5. 检查请求合法性，不合法返回400
 * 6. HEAD请求仅设置Content-Length
 * 7. GET请求读取文件并返回
 */
bool StaticFileHandler::handle(const HttpRequest &req, HttpResponse *resp, const std::string &baseDir)
{
    auto setHtmlError = [&](HttpResponse::HttpStatusCode code,
                            const std::string &statusMessage,
                            const std::string &errorPageName,
                            const std::string &fallbackBody)
    {
        resp->setStatusCode(code);
        resp->setStatusMessage(statusMessage);
        resp->setContentType("text/html");

        const std::string errorPath = baseDir + errorPageName;
        if (req.getMethod() == HttpRequest::Method::kHead)
        {
            struct stat errorSt;
            if (::stat(errorPath.c_str(), &errorSt) == 0 && S_ISREG(errorSt.st_mode))
            {
                resp->setContentLength(errorSt.st_size);
            }
            else
            {
                resp->setContentLength(0);
            }
            return;
        }

        resp->setBody(readFileOrDefault(errorPath, fallbackBody));
    };

    // 0. 协议完备性：只允许GET和HEAD方法
    if (req.getMethod() != HttpRequest::Method::kGet && req.getMethod() != HttpRequest::Method::kHead)
    {
        resp->setStatusCode(HttpResponse::k405MethodNotAllowed);
        resp->setStatusMessage("Method Not Allowed");
        resp->setHeader("Allow", "GET, HEAD");
        resp->setBody("<html><body><h1>405 Method Not Allowed</h1></body></html>");
        return true;
    }

    // 1. 路径映射，将URL路径映射到本地文件
    std::string urlPath = req.getPath();

    // URL解码以处理编码的路径穿越攻击（如%2e%2e%2f）
    std::string decodedPath;
    for (size_t i = 0; i < urlPath.length(); ++i)
    {
        if (urlPath[i] == '%' && i + 2 < urlPath.length())
        {
            int value;
            std::istringstream is(urlPath.substr(i + 1, 2));
            if (is >> std::hex >> value)
            {
                decodedPath += static_cast<char>(value);
                i += 2;
            }
            else
            {
                decodedPath += urlPath[i];
            }
        }
        else
        {
            decodedPath += urlPath[i];
        }
    }

    // 检查路径穿越攻击（..）
    if (decodedPath.find("..") != std::string::npos)
    {
        DLOG_WARN << "[StaticFileHandler] 检测到路径穿越攻击: " << decodedPath;
        setHtmlError(HttpResponse::k403Forbidden, "Forbidden", "/403.html",
                     "<html><body><h1>403 Forbidden</h1></body></html>");
        return true;
    }

    if (decodedPath == "/")
        decodedPath = "/index.html"; // 默认首页

    std::string filePath = baseDir + decodedPath;

    // 解析为规范路径并验证是否在基础目录内
    char resolvedBase[PATH_MAX];
    char resolvedFile[PATH_MAX];
    if (realpath(baseDir.c_str(), resolvedBase) == nullptr)
    {
        DLOG_ERROR << "[StaticFileHandler] 无法解析基础目录: " << baseDir;
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setBody("<html><body><h1>500 Internal Server Error</h1></body></html>");
        return true;
    }

    if (realpath(filePath.c_str(), resolvedFile) != nullptr)
    {
        // 检查解析后的路径是否在基础目录内
        std::string baseStr(resolvedBase);
        std::string fileStr(resolvedFile);

        // 确保baseStr以/结尾，避免/var/www匹配/var/www2的问题
        if (!baseStr.empty() && baseStr.back() != '/')
            baseStr += '/';

        // 检查fileStr是否以baseStr开头，或者fileStr等于baseStr去掉末尾/
        bool isInBase = false;
        if (!baseStr.empty())
        {
            if (fileStr.rfind(baseStr, 0) == 0)
            {
                isInBase = true;
            }
            else if (baseStr.length() > 1 && fileStr == baseStr.substr(0, baseStr.length() - 1))
            {
                isInBase = true;
            }
        }

        if (!isInBase)
        {
            DLOG_WARN << "[StaticFileHandler] 路径穿越已阻止: " << fileStr << " 不在 " << baseStr << " 内";
            setHtmlError(HttpResponse::k403Forbidden, "Forbidden", "/403.html",
                         "<html><body><h1>403 Forbidden</h1></body></html>");
            return true;
        }
        filePath = resolvedFile;
    }

    DLOG_INFO << "[StaticFileHandler] 处理静态资源请求: " << filePath;

    struct stat st;
    // 2. 检查文件是否存在
    if (stat(filePath.c_str(), &st) != 0)
    {
        DLOG_WARN << "[StaticFileHandler] 文件不存在: " << filePath;
        setHtmlError(HttpResponse::k404NotFound, "Not Found", "/404.html",
                     "<html><body><h1>404 Not Found</h1></body></html>");
        return true;
    }
    // 3. 检查文件权限
    if (!(st.st_mode & S_IROTH))
    {
        DLOG_WARN << "[StaticFileHandler] 文件无权限访问: " << filePath;
        setHtmlError(HttpResponse::k403Forbidden, "Forbidden", "/403.html",
                     "<html><body><h1>403 Forbidden</h1></body></html>");
        return true;
    }
    // 4. 检查请求是否合法（如方法、协议版本）
    if (req.getMethod() == HttpRequest::Method::kInvalid || req.getVersion() == HttpRequest::Version::kUnknown)
    {
        DLOG_WARN << "[StaticFileHandler] 非法请求: method=" << static_cast<int>(req.getMethod()) << ", version=" << static_cast<int>(req.getVersion());
        setHtmlError(HttpResponse::k400BadRequest, "Bad Request", "/400.html",
                     "<html><body><h1>400 Bad Request</h1></body></html>");
        return true;
    }
    // 5. 读取文件内容
    // 对于HEAD请求，只需要设置Content-Length，不读取文件内容
    if (req.getMethod() == HttpRequest::Method::kHead)
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType(getMimeType(filePath));
        resp->setContentLength(st.st_size);
        return true;
    }

    // GET请求：读取文件内容到内存
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs)
    {
        DLOG_ERROR << "[StaticFileHandler] 无法读取文件: " << filePath;
        setHtmlError(HttpResponse::k500InternalServerError, "Internal Server Error", "/500.html",
                     "<html><body><h1>500 Internal Server Error</h1></body></html>");
        return true;
    }

    std::string body((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType(getMimeType(filePath));
    resp->setBody(body);
    return true;
}
