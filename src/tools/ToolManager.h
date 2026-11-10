#pragma once

#include <nlohmann/json.hpp>
#include <string>

class ToolManager
{
public:
    ToolManager() = default;

    nlohmann::json getDeclarations() const;

    nlohmann::json execute(const std::string &name,
                           const nlohmann::json &arguments) const;

private:
    nlohmann::json getSystemInfo() const;
    nlohmann::json getDiskUsage() const;
    nlohmann::json listDirectory(const nlohmann::json &arguments) const;
};
