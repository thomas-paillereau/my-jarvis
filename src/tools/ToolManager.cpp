#include "ToolManager.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/statvfs.h>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

json ToolManager::getDeclarations() const
{
    return json::array(
        { { { "name", "get_system_info" },
            { "description",
              "Get basic information about the Linux system, CPU and memory." },
            { "parameters",
              { { "type", "OBJECT" },
                { "properties", json::object() },
                { "required", json::array() } } } },
          { { "name", "get_disk_usage" },
            { "description", "Get disk space usage for the root filesystem." },
            { "parameters",
              { { "type", "OBJECT" },
                { "properties", json::object() },
                { "required", json::array() } } } },
          { { "name", "list_directory" },
            { "description",
              "List the contents of a directory inside the user's home "
              "directory. Defaults to the home directory." },
            { "parameters",
              { { "type", "OBJECT" },
                { "properties",
                  { { "path",
                      { { "type", "STRING" },
                        { "description",
                          "Absolute or relative directory path inside the "
                          "user's home directory." } } } } },
                { "required", json::array() } } } } });
}

json ToolManager::execute(const std::string &name, const json &arguments) const
{
    if (name == "get_system_info")
        return getSystemInfo();

    if (name == "get_disk_usage")
        return getDiskUsage();

    if (name == "list_directory")
        return listDirectory(arguments);

    return { { "error", "Unknown tool: " + name } };
}

json ToolManager::getSystemInfo() const
{
    json result;

    // Distribution information.
    std::ifstream osFile("/etc/os-release");
    std::string line;

    while (std::getline(osFile, line))
    {
        if (line.rfind("PRETTY_NAME=", 0) == 0)
        {
            result["operating_system"] = line.substr(12);

            break;
        }
    }

    // CPU information.
    std::ifstream cpuFile("/proc/cpuinfo");

    while (std::getline(cpuFile, line))
    {
        if (line.rfind("model name", 0) == 0)
        {
            const auto separator = line.find(':');

            if (separator != std::string::npos)
                result["cpu"] = line.substr(separator + 2);

            break;
        }
    }

    // Memory information.
    std::ifstream memFile("/proc/meminfo");

    unsigned long long total = 0;
    unsigned long long available = 0;

    while (std::getline(memFile, line))
    {
        if (line.rfind("MemTotal:", 0) == 0)
            total = std::stoull(line.substr(9));

        else if (line.rfind("MemAvailable:", 0) == 0)
            available = std::stoull(line.substr(13));
    }

    result["memory_total_mb"] = total / 1024;
    result["memory_available_mb"] = available / 1024;
    result["memory_used_mb"] = (total - available) / 1024;

    return result;
}

json ToolManager::getDiskUsage() const
{
    struct statvfs stats{};

    if (statvfs("/", &stats) != 0)
        return { { "error", "Unable to retrieve disk usage." } };

    const unsigned long long total =
        static_cast<unsigned long long>(stats.f_blocks) * stats.f_frsize;

    const unsigned long long available =
        static_cast<unsigned long long>(stats.f_bavail) * stats.f_frsize;

    const unsigned long long used = total - available;

    return { { "filesystem", "/" },
             { "total_gb", total / (1024ULL * 1024 * 1024) },
             { "used_gb", used / (1024ULL * 1024 * 1024) },
             { "available_gb", available / (1024ULL * 1024 * 1024) } };
}

json ToolManager::listDirectory(const json &arguments) const
{
    const char *homeEnv = std::getenv("HOME");

    if (!homeEnv)
        return { { "error", "HOME environment variable is not set." } };

    try
    {
        const fs::path home = fs::canonical(homeEnv);

        fs::path requested = home;

        if (arguments.contains("path") && arguments["path"].is_string())
        {
            requested = arguments["path"].get<std::string>();

            if (requested.is_relative())
                requested = home / requested;
        }

        const fs::path target = fs::canonical(requested);

        // Restrict access to the user's home directory.
        auto relative = target.lexically_relative(home);

        if (target != home && (relative.empty() || *relative.begin() == ".."))
        {
            return { { "error",
                       "Access outside the home directory is not allowed." } };
        }

        if (!fs::is_directory(target))
            return { { "error", "The requested path is not a directory." } };

        json entries = json::array();

        for (const auto &entry : fs::directory_iterator(target))
        {
            entries.push_back(
                { { "name", entry.path().filename().string() },
                  { "type", entry.is_directory() ? "directory" : "file" } });
        }

        return { { "path", target.string() }, { "entries", entries } };
    }
    catch (const fs::filesystem_error &e)
    {
        return { { "error", e.what() } };
    }
}
