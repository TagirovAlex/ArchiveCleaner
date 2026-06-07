#pragma once
#include <string>
#include <vector>
#include <map>

class Config {
public:
    Config() = default;
    bool load(const std::string& path = "");

    std::vector<std::string> folders() const;
    std::string mailServer() const;
    int mailPort() const;
    bool mailAuth() const;
    std::string mailUsername() const;
    std::string mailPassword() const;
    std::string senderName() const;
    std::string senderEmail() const;
    std::vector<std::string> recipients() const;
    std::string templatePath() const;
    std::string logPath() const;
    int maxLogs() const;

    static std::string executableDir();

private:
    std::string get(const std::string& section, const std::string& key, const std::string& defaultVal = "") const;
    bool getBool(const std::string& section, const std::string& key, bool defaultVal = false) const;
    int getInt(const std::string& section, const std::string& key, int defaultVal = 0) const;

    std::map<std::string, std::map<std::string, std::string>> m_data;
    std::string m_configDir;
};
