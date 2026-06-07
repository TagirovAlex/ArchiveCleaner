#include "Config.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
}

bool Config::load(const std::string& path) {
    m_data.clear();

    std::string configPath = path;
    if (configPath.empty()) {
        configPath = executableDir() + "\\config.ini";
    }

    std::ifstream file(configPath);
    if (!file.is_open()) return false;

    // Determine config directory for relative template paths
    auto lastSep = configPath.find_last_of("\\/");
    m_configDir = (lastSep != std::string::npos) ? configPath.substr(0, lastSep) : ".";

    std::string currentSection;
    std::string line;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);

        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;

        // Line continuation
        if (trimmed[0] == ' ' || trimmed[0] == '\t') {
            if (!currentSection.empty() && !m_data[currentSection].empty()) {
                auto& lastVal = m_data[currentSection].rbegin()->second;
                lastVal += "\n" + trimmed;
            }
            continue;
        }

        if (trimmed[0] == '[') {
            auto end = trimmed.find(']');
            if (end != std::string::npos) {
                currentSection = trim(trimmed.substr(1, end - 1));
            }
            continue;
        }

        auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));

        if (key.empty()) continue;

        m_data[currentSection][key] = value;
    }

    return true;
}

std::string Config::executableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string fullPath(path);
    auto lastSep = fullPath.find_last_of("\\/");
    if (lastSep != std::string::npos) {
        return fullPath.substr(0, lastSep);
    }
    return ".";
}

std::vector<std::string> Config::folders() const {
    std::vector<std::string> result;
    std::string raw = get("General", "Folders");
    if (raw.empty()) return result;

    std::string current;
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == ',' || c == ';' || c == '\n') {
            std::string folder = trim(current);
            if (!folder.empty()) result.push_back(folder);
            current.clear();
        } else {
            current += c;
        }
    }
    std::string folder = trim(current);
    if (!folder.empty()) result.push_back(folder);

    return result;
}

std::string Config::mailServer() const {
    return get("Mail", "Server");
}

int Config::mailPort() const {
    return getInt("Mail", "Port", 25);
}

bool Config::mailAuth() const {
    return getBool("Mail", "Auth", false);
}

std::string Config::mailUsername() const {
    return get("Mail", "Username");
}

std::string Config::mailPassword() const {
    return get("Mail", "Password");
}

std::string Config::senderName() const {
    return get("Mail", "SenderName", "Archive Cleaner Service");
}

std::string Config::senderEmail() const {
    return get("Mail", "SenderEmail");
}

std::vector<std::string> Config::recipients() const {
    std::vector<std::string> result;
    std::string raw = get("Mail", "Recipients");
    if (raw.empty()) return result;

    std::stringstream ss(raw);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = trim(item);
        if (!t.empty()) result.push_back(t);
    }
    return result;
}

std::string Config::templatePath() const {
    std::string tpl = get("Mail", "TemplatePath", "report_template.html");
    if (!tpl.empty() && tpl.find_first_of("\\/") == std::string::npos) {
        tpl = m_configDir + "\\" + tpl;
    }
    return tpl;
}

// Private helpers

std::string Config::get(const std::string& section, const std::string& key, const std::string& defaultVal) const {
    auto secIt = m_data.find(section);
    if (secIt == m_data.end()) return defaultVal;
    auto keyIt = secIt->second.find(key);
    if (keyIt == secIt->second.end()) return defaultVal;
    return keyIt->second;
}

bool Config::getBool(const std::string& section, const std::string& key, bool defaultVal) const {
    std::string val = get(section, key);
    if (val.empty()) return defaultVal;
    std::string lower;
    for (char c : val) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower == "true" || lower == "yes" || lower == "1";
}

int Config::getInt(const std::string& section, const std::string& key, int defaultVal) const {
    std::string val = get(section, key);
    if (val.empty()) return defaultVal;
    return std::stoi(val);
}
