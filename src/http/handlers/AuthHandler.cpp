#include "http/handlers/AuthHandler.h"
#include "base/ConfigManager.h"
#include "log/LogManager.h"
#include <jwt-cpp/jwt.h>

bool checkAuth(const HttpRequest &req, int &user_id)
{
    auto authOpt = req.getHeader("Authorization");
    if (!authOpt.has_value())
        return false;

    const std::string &auth = authOpt.value();
    if (auth.size() > 7 && auth.substr(0, 7) == "Bearer ")
    {
        std::string token = auth.substr(7);
        if (token.empty())
            return false;
        try
        {
            auto baseConfig = ConfigManager::getInstance().getBaseConfig();
            if (!baseConfig)
                return false;

            auto decoded = jwt::decode(token);

            auto verifier = jwt::verify()
                                .allow_algorithm(jwt::algorithm::hs256(baseConfig->getJwtSecret()))
                                .with_issuer(baseConfig->getJwtIssuer());

            verifier.verify(decoded);

            if (decoded.has_expires_at())
            {
                auto exp = decoded.get_expires_at();
                if (exp < std::chrono::system_clock::now())
                    return false;
            }
            else
            {
                return false;
            }

            user_id = std::stoi(decoded.get_payload_claim("user_id").as_string());
            return true;
        }
        catch (const std::exception &e)
        {
            DLOG_WARN << "[Auth] JWT验证失败: " << e.what();
            return false;
        }
        catch (...)
        {
            DLOG_WARN << "[Auth] JWT验证失败: 未知错误";
            return false;
        }
    }
    return false;
}
