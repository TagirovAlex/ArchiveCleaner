#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <windows.h>
#include "Mailer.h"

namespace fs = std::filesystem;

// --- INI Parser ---
struct IniConfig {
    std::map<std::string, std::map<std::string, std::string>> sections;

    bool load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::string currentSection;
        std::string line;

        while (std::getline(file, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            size_t end = line.find_last_not_of(" \t\r\n");
            if (end == std::string::npos) continue;
            std::string trimmed = line.substr(start, end - start + 1);

            if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;

            if (trimmed[0] == '[') {
                auto close = trimmed.find(']');
                if (close != std::string::npos) {
                    currentSection = trimmed.substr(1, close - 1);
                    size_t s = currentSection.find_first_not_of(" \t");
                    size_t e = currentSection.find_last_not_of(" \t");
                    if (s != std::string::npos && e != std::string::npos)
                        currentSection = currentSection.substr(s, e - s + 1);
                }
                continue;
            }

            // Line continuation: if line starts with whitespace and we have a previous key, append
            if (trimmed[0] == ' ' || trimmed[0] == '\t') {
                if (!currentSection.empty() && !sections[currentSection].empty()) {
                    auto& lastVal = sections[currentSection].rbegin()->second;
                    lastVal += "\n" + trimmed;
                }
                continue;
            }

            auto eq = trimmed.find('=');
            if (eq == std::string::npos) continue;

            std::string key = trimmed.substr(0, eq);
            std::string value = trimmed.substr(eq + 1);

            size_t ks = key.find_first_not_of(" \t");
            size_t ke = key.find_last_not_of(" \t");
            if (ks != std::string::npos && ke != std::string::npos)
                key = key.substr(ks, ke - ks + 1);

            size_t vs = value.find_first_not_of(" \t");
            size_t ve = value.find_last_not_of(" \t");
            if (vs != std::string::npos && ve != std::string::npos)
                value = value.substr(vs, ve - vs + 1);

            if (key.empty()) continue;

            sections[currentSection][key] = value;
        }

        return true;
    }

    std::string get(const std::string& section, const std::string& key, const std::string& defaultVal = "") const {
        auto secIt = sections.find(section);
        if (secIt == sections.end()) return defaultVal;
        auto keyIt = secIt->second.find(key);
        if (keyIt == secIt->second.end()) return defaultVal;
        return keyIt->second;
    }

    bool getBool(const std::string& section, const std::string& key, bool defaultVal = false) const {
        std::string val = get(section, key);
        if (val.empty()) return defaultVal;
        std::string lower;
        for (char c : val) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return lower == "true" || lower == "yes" || lower == "1";
    }

    int getInt(const std::string& section, const std::string& key, int defaultVal = 0) const {
        std::string val = get(section, key);
        if (val.empty()) return defaultVal;
        return std::stoi(val);
    }

    std::vector<std::string> getFolders() const {
        std::vector<std::string> result;
        std::string raw = get("General", "Folders");
        if (raw.empty()) return result;

        // Split by comma, semicolon, or newline
        std::string current;
        for (size_t i = 0; i < raw.size(); ++i) {
            char c = raw[i];
            if (c == ',' || c == ';' || c == '\n') {
                size_t s = current.find_first_not_of(" \t\r");
                size_t e = current.find_last_not_of(" \t\r");
                if (s != std::string::npos && e != std::string::npos) {
                    std::string folder = current.substr(s, e - s + 1);
                    if (!folder.empty()) result.push_back(folder);
                }
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            size_t s = current.find_first_not_of(" \t\r");
            size_t e = current.find_last_not_of(" \t\r");
            if (s != std::string::npos && e != std::string::npos) {
                std::string folder = current.substr(s, e - s + 1);
                if (!folder.empty()) result.push_back(folder);
            }
        }

        return result;
    }

    std::vector<std::string> getRecipients() const {
        std::vector<std::string> result;
        std::string raw = get("Mail", "Recipients");
        if (raw.empty()) return result;

        std::string current;
        for (size_t i = 0; i < raw.size(); ++i) {
            char c = raw[i];
            if (c == ',' || c == ';') {
                size_t s = current.find_first_not_of(" \t\r");
                size_t e = current.find_last_not_of(" \t\r");
                if (s != std::string::npos && e != std::string::npos) {
                    std::string r = current.substr(s, e - s + 1);
                    if (!r.empty()) result.push_back(r);
                }
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            size_t s = current.find_first_not_of(" \t\r");
            size_t e = current.find_last_not_of(" \t\r");
            if (s != std::string::npos && e != std::string::npos) {
                std::string r = current.substr(s, e - s + 1);
                if (!r.empty()) result.push_back(r);
            }
        }

        return result;
    }
};

// --- Legacy config readers (fallback) ---
bool LoadFoldersFromLegacyFile(std::vector<std::string>& folders) {
    std::ifstream file("folders.txt");
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        size_t e = line.find_last_not_of(" \n\r\t");
        if (e == std::string::npos) continue;
        line = line.substr(0, e + 1);
        if (!line.empty()) folders.push_back(line);
    }
    return !folders.empty();
}

struct LegacyMailConfig {
    std::string smtpServer;
    int smtpPort = 587;
    std::string username;
    std::string password;
    std::string fromEmail;
    std::string toEmail;
    bool useSSL = true;
};

bool LoadLegacyMailConfig(LegacyMailConfig& config) {
    std::ifstream file("email_config.txt");
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        if (end == std::string::npos) continue;
        line = line.substr(start, end - start + 1);
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "SMTP_SERVER") config.smtpServer = value;
        else if (key == "SMTP_PORT") config.smtpPort = std::stoi(value);
        else if (key == "USERNAME") config.username = value;
        else if (key == "PASSWORD") config.password = value;
        else if (key == "FROM_EMAIL") config.fromEmail = value;
        else if (key == "TO_EMAIL") config.toEmail = value;
        else if (key == "USE_SSL") config.useSSL = (value == "true" || value == "1" || value == "yes");
    }
    return !config.smtpServer.empty() && !config.toEmail.empty();
}

// --- Helpers ---
std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm parts;
#ifdef _MSC_VER
    localtime_s(&parts, &now_c);
#else
    parts = *std::localtime(&now_c);
#endif
    std::stringstream ss;
    ss << std::put_time(&parts, "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string GetFormattedTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm parts;
#ifdef _MSC_VER
    localtime_s(&parts, &now_c);
#else
    parts = *std::localtime(&now_c);
#endif
    std::stringstream ss;
    ss << std::put_time(&parts, "%d.%m.%Y %H:%M:%S");
    return ss.str();
}

std::string GetYearMonth(fs::file_time_type ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm parts;
#ifdef _MSC_VER
    localtime_s(&parts, &tt);
#else
    parts = *std::localtime(&tt);
#endif
    std::stringstream ss;
    ss << std::put_time(&parts, "%Y-%m");
    return ss.str();
}

std::string EscapeHtml(const std::string& text) {
    std::string result;
    for (unsigned char c : text) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            default: result += c;
        }
    }
    return result;
}

std::string FormatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return ss.str();
}

std::string GetDriveRoot(const std::string& path) {
    if (path.size() >= 2 && path[1] == ':') {
        return path.substr(0, 2) + "\\";
    }
    return "";
}

uint64_t GetDriveFreeSpace(const std::string& path) {
    std::string root = GetDriveRoot(path);
    if (root.empty()) return 0;
    ULARGE_INTEGER freeBytes;
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytes, nullptr, nullptr)) {
        return freeBytes.QuadPart;
    }
    return 0;
}

std::string FormatDuration(std::chrono::seconds dur) {
    auto h = std::chrono::duration_cast<std::chrono::hours>(dur);
    auto m = std::chrono::duration_cast<std::chrono::minutes>(dur - h);
    auto s = std::chrono::duration_cast<std::chrono::seconds>(dur - h - m);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << h.count() << ":"
       << std::setfill('0') << std::setw(2) << m.count() << ":"
       << std::setfill('0') << std::setw(2) << s.count();
    return ss.str();
}

// --- Template loading and report generation ---
bool LoadHtmlTemplate(std::string& templateContent) {
    std::ifstream file("report_template.html");
    if (!file.is_open()) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    templateContent = ss.str();
    return !templateContent.empty();
}

void ReplaceAll(std::string& str, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

struct DeleteRecord {
    std::string folder;
    std::string fileName;
};

std::string GenerateTableRows(const std::vector<DeleteRecord>& records) {
    if (records.empty()) {
        return "<tr><td colspan=\"2\" style=\"padding:6px 8px;border:1px solid #dddddd;text-align:center;font-size:12px;font-family:'Segoe UI',Arial,sans-serif;\">\u041D\u0435\u0442 \u0443\u0434\u0430\u043B\u0435\u043D\u043D\u044B\u0445 \u0444\u0430\u0439\u043B\u043E\u0432</td></tr>\n";
    }
    std::string rows;
    for (const auto& rec : records) {
        rows += "<tr>\n<td style=\"padding:6px 8px;border:1px solid #dddddd;font-size:12px;font-family:'Segoe UI',Arial,sans-serif;\">"
                + EscapeHtml(rec.folder) + "</td>\n<td style=\"padding:6px 8px;border:1px solid #dddddd;font-size:12px;font-family:'Segoe UI',Arial,sans-serif;\">"
                + EscapeHtml(rec.fileName) + "</td>\n</tr>\n";
    }
    return rows;
}

std::string GenerateDriveSpaceRows(const std::map<std::string, uint64_t>& before,
                                    const std::map<std::string, uint64_t>& after) {
    std::vector<std::string> drives;
    for (const auto& [d, _] : before) drives.push_back(d);
    for (const auto& [d, _] : after) {
        if (std::find(drives.begin(), drives.end(), d) == drives.end()) {
            drives.push_back(d);
        }
    }

    if (drives.empty()) return "";

    std::string rows;
    for (const auto& drive : drives) {
        uint64_t b = 0, a = 0;
        auto it = before.find(drive);
        if (it != before.end()) b = it->second;
        it = after.find(drive);
        if (it != after.end()) a = it->second;

        rows += "<tr>\n<td style=\"padding:6px 8px;border:1px solid #dddddd;font-family:'Segoe UI',Arial,sans-serif;\">\u0421\u0432\u043E\u0431\u043E\u0434\u043D\u043E \u043D\u0430 " + EscapeHtml(drive) + "</td>\n<td style=\"padding:6px 8px;border:1px solid #dddddd;font-family:'Segoe UI',Arial,sans-serif;\">\u0434\u043E: " + FormatBytes(b) + " &rArr; \u043F\u043E\u0441\u043B\u0435: " + FormatBytes(a) + "</td>\n</tr>\n";
    }
    return rows;
}

int main(int argc, char* argv[]) {
    // --- Load config ---
    IniConfig config;
    bool configLoaded = config.load("config.ini");

    bool debugMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debugMode = true;
            break;
        }
    }

    if (!configLoaded) {
        // Fallback: read debug from command line only
    }

    // --- Get folders ---
    std::vector<std::string> folders;
    if (configLoaded) {
        folders = config.getFolders();
    }
    if (folders.empty()) {
        LoadFoldersFromLegacyFile(folders);
    }

    // --- Setup logging ---
    std::string logFileName = "log_" + GetCurrentTimestamp() + ".txt";
    std::ofstream logFile(logFileName);
    if (!logFile.is_open()) {
        std::cerr << "Could not create log file!" << std::endl;
        return 1;
    }

    std::string debugLogName = "debug_delete_list.txt";
    std::ofstream debugFile;

    if (debugMode) {
        debugFile.open(debugLogName);
        logFile << "[INFO] DEBUG MODE ENABLED" << std::endl;
        logFile << "[INFO] Deletions will be logged to " << debugLogName << std::endl;
    } else {
        logFile << "[INFO] NORMAL MODE: Files will be deleted." << std::endl;
    }

    logFile << "[INFO] Script started at " << GetCurrentTimestamp() << std::endl;

    if (folders.empty()) {
        logFile << "[ERROR] No folders specified. Check config.ini or folders.txt" << std::endl;
        logFile.close();
        return 1;
    }

    // --- Tracking ---
    std::map<std::string, uint64_t> freeSpaceBefore;
    std::map<std::string, uint64_t> freeSpaceAfter;
    std::vector<DeleteRecord> deleteRecords;
    auto startTime = std::chrono::steady_clock::now();

    // --- Process folders ---
    for (const auto& folderPath : folders) {
        std::string path = folderPath;
        // Trim trailing backslash for consistency
        while (!path.empty() && (path.back() == '\\' || path.back() == '/'))
            path.pop_back();

        std::string drive = GetDriveRoot(path);
        if (!drive.empty() && freeSpaceBefore.find(drive) == freeSpaceBefore.end()) {
            freeSpaceBefore[drive] = GetDriveFreeSpace(drive);
            logFile << "[INFO] Free space on " << drive << " before: " << FormatBytes(freeSpaceBefore[drive]) << std::endl;
        }

        try {
            logFile << "[INFO] Processing folder: " << path << std::endl;

            std::map<std::string, std::vector<fs::path>> monthlyGroups;

            if (fs::exists(path) && fs::is_directory(path)) {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        std::string ym = GetYearMonth(entry.last_write_time());
                        monthlyGroups[ym].push_back(entry.path());
                    }
                }
            } else {
                logFile << "[WARNING] Path does not exist or is not a directory: " << path << std::endl;
                continue;
            }

            for (auto& [month, files] : monthlyGroups) {
                if (files.empty()) continue;

                std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
                    return fs::last_write_time(a) < fs::last_write_time(b);
                });

                std::vector<fs::path> toKeep;
                if (files.size() == 1) {
                    toKeep.push_back(files[0]);
                } else {
                    toKeep.push_back(files[0]);
                    toKeep.push_back(files.back());
                }

                for (const auto& file : files) {
                    bool keep = false;
                    for (const auto& k : toKeep) {
                        if (file == k) {
                            keep = true;
                            break;
                        }
                    }

                    if (!keep) {
                        if (debugMode) {
                            debugFile << "Delete: " << file.string() << std::endl;
                            logFile << "[DEBUG] Marked for deletion: " << file.filename().string() << std::endl;
                            deleteRecords.push_back({path + "\\", file.filename().string()});
                        } else {
                            try {
                                fs::remove(file);
                                logFile << "[SUCCESS] Deleted: " << file.filename().string() << std::endl;
                                deleteRecords.push_back({path + "\\", file.filename().string()});
                            } catch (const fs::filesystem_error& e) {
                                logFile << "[ERROR] Failed to delete " << file.filename().string() << ": " << e.what() << std::endl;
                            }
                        }
                    } else {
                        logFile << "[KEEP] " << file.filename().string() << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            logFile << "[ERROR] Exception in folder " << path << ": " << e.what() << std::endl;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

    for (const auto& [drive, _] : freeSpaceBefore) {
        freeSpaceAfter[drive] = GetDriveFreeSpace(drive);
        logFile << "[INFO] Free space on " << drive << " after: " << FormatBytes(freeSpaceAfter[drive]) << std::endl;
    }

    logFile << "[INFO] Total execution time: " << FormatDuration(duration) << std::endl;
    logFile << "[INFO] Total files deleted/marked: " << deleteRecords.size() << std::endl;

    // --- Send email ---
    std::string mailServer;
    int mailPort = 25;
    bool mailAuth = false;
    std::string mailUsername;
    std::string mailPassword;
    std::string senderName;
    std::string senderEmail;
    std::vector<std::string> recipients;
    std::string templatePath = "report_template.html";
    bool mailConfigured = false;

    if (configLoaded) {
        mailServer = config.get("Mail", "Server");
        mailPort = config.getInt("Mail", "Port", 25);
        mailAuth = config.getBool("Mail", "Auth", false);
        mailUsername = config.get("Mail", "Username");
        mailPassword = config.get("Mail", "Password");
        senderName = config.get("Mail", "SenderName", "Archive Cleaner Service");
        senderEmail = config.get("Mail", "SenderEmail");
        recipients = config.getRecipients();
        templatePath = config.get("Mail", "TemplatePath", "report_template.html");
        mailConfigured = !mailServer.empty() && !recipients.empty();
    }

    // Fallback to legacy email_config.txt
    if (!mailConfigured) {
        LegacyMailConfig legacy;
        if (LoadLegacyMailConfig(legacy)) {
            mailServer = legacy.smtpServer;
            mailPort = legacy.smtpPort;
            senderEmail = legacy.fromEmail;
            senderName = "Archive Cleaner Service";
            if (!legacy.toEmail.empty()) recipients.push_back(legacy.toEmail);
            mailAuth = !legacy.username.empty();
            mailUsername = legacy.username;
            mailPassword = legacy.password;
            mailConfigured = true;
        }
    }

    if (mailConfigured && !senderEmail.empty()) {
        logFile << "[INFO] Sending email report..." << std::endl;

        std::string templateContent;
        if (LoadHtmlTemplate(templateContent)) {
            ReplaceAll(templateContent, "{{TIMESTAMP}}", GetFormattedTimestamp());
            ReplaceAll(templateContent, "{{MODE}}", debugMode ? "DEBUG (\u0431\u0435\u0437 \u0443\u0434\u0430\u043B\u0435\u043D\u0438\u044F)" : "NORMAL");
            ReplaceAll(templateContent, "{{TOTAL_DELETED}}", std::to_string(deleteRecords.size()));
            ReplaceAll(templateContent, "{{TABLE_ROWS}}", GenerateTableRows(deleteRecords));
            ReplaceAll(templateContent, "{{DURATION}}", FormatDuration(duration));
            ReplaceAll(templateContent, "{{DRIVE_SPACE_ROWS}}", GenerateDriveSpaceRows(freeSpaceBefore, freeSpaceAfter));

            std::string subject = "ArchiveCleaner Report - " + GetCurrentTimestamp();

            Mailer mailer;
            bool mailOk = true;

            if (!mailer.connect(mailServer, mailPort)) {
                logFile << "[ERROR] Failed to connect to mail server: " << mailServer << std::endl;
                mailOk = false;
            } else {
                if (!mailer.sendMail(senderName, senderEmail, recipients, subject, templateContent,
                                     mailAuth, mailUsername, mailPassword)) {
                    logFile << "[ERROR] Failed to send email" << std::endl;
                    mailOk = false;
                }
                mailer.disconnect();
            }

            if (mailOk)
                logFile << "[SUCCESS] Email report sent to " << recipients.size() << " recipient(s)" << std::endl;
        } else {
            logFile << "[ERROR] Could not load template: " << templatePath << std::endl;
        }
    } else {
        logFile << "[INFO] Mail not configured, skipping email report" << std::endl;
    }

    logFile << "[INFO] Script finished." << std::endl;
    logFile.close();
    if (debugFile.is_open()) debugFile.close();

    return 0;
}
