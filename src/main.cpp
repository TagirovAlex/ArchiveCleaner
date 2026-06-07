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
#include "Config.h"
#include "Mailer.h"

namespace fs = std::filesystem;

static std::string GetCurrentTimestamp() {
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

static std::string GetFormattedTimestamp() {
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

static std::string GetYearMonth(fs::file_time_type ftime) {
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

static std::string EscapeHtml(const std::string& text) {
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

static std::string FormatBytes(uint64_t bytes) {
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

static std::string GetDriveRoot(const std::string& path) {
    if (path.size() >= 2 && path[1] == ':') {
        return path.substr(0, 2) + "\\";
    }
    return "";
}

static uint64_t GetDriveFreeSpace(const std::string& path) {
    std::string root = GetDriveRoot(path);
    if (root.empty()) return 0;
    ULARGE_INTEGER freeBytes;
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytes, nullptr, nullptr)) {
        return freeBytes.QuadPart;
    }
    return 0;
}

static std::string FormatDuration(std::chrono::seconds dur) {
    auto h = std::chrono::duration_cast<std::chrono::hours>(dur);
    auto m = std::chrono::duration_cast<std::chrono::minutes>(dur - h);
    auto s = std::chrono::duration_cast<std::chrono::seconds>(dur - h - m);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << h.count() << ":"
       << std::setfill('0') << std::setw(2) << m.count() << ":"
       << std::setfill('0') << std::setw(2) << s.count();
    return ss.str();
}

static bool LoadHtmlTemplate(const std::string& path, std::string& templateContent) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    templateContent = ss.str();
    return !templateContent.empty();
}

static void ReplaceAll(std::string& str, const std::string& from, const std::string& to) {
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

static std::string GenerateTableRows(const std::vector<DeleteRecord>& records) {
    if (records.empty()) {
        return "<tr><td colspan=\"2\" style=\"text-align:center;color:#888;\">\u041D\u0435\u0442 \u0443\u0434\u0430\u043B\u0451\u043D\u043D\u044B\u0445 \u0444\u0430\u0439\u043B\u043E\u0432</td></tr>\n";
    }
    std::string rows;
    for (const auto& rec : records) {
        rows += "<tr>\n<td>" + EscapeHtml(rec.folder) + "</td>\n<td>" + EscapeHtml(rec.fileName) + "</td>\n</tr>\n";
    }
    return rows;
}

static std::string GenerateDriveSpaceRows(const std::map<std::string, uint64_t>& before,
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

        rows += "<tr>\n<th>\u0421\u0432\u043E\u0431\u043E\u0434\u043D\u043E \u043D\u0430 " + EscapeHtml(drive) + "</th>\n<td>\u0434\u043E: " + FormatBytes(b) + " &rArr; \u043F\u043E\u0441\u043B\u0435: " + FormatBytes(a) + "</td>\n</tr>\n";
    }
    return rows;
}

int main(int argc, char* argv[]) {
    bool debugMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debugMode = true;
        }
    }

    // Load config
    std::string configPath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configPath = argv[++i];
            }
        }
    }

    Config config;
    bool configLoaded = false;

    // Try specified config path, then default
    if (!configPath.empty()) {
        configLoaded = config.load(configPath);
    }
    if (!configLoaded) {
        configLoaded = config.load(); // defaults to config.ini next to exe
    }

    // Fallback: folders.txt for backward compatibility
    auto folders = config.folders();
    if (folders.empty()) {
        std::ifstream legacyFile("folders.txt");
        if (legacyFile.is_open()) {
            std::string line;
            while (std::getline(legacyFile, line)) {
                size_t e = line.find_last_not_of(" \n\r\t");
                if (e == std::string::npos) continue;
                line = line.substr(0, e + 1);
                if (!line.empty()) folders.push_back(line);
            }
        }
    }

    // Logging
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

    // Tracking
    std::map<std::string, uint64_t> freeSpaceBefore;
    std::map<std::string, uint64_t> freeSpaceAfter;
    std::vector<DeleteRecord> deleteRecords;
    auto startTime = std::chrono::steady_clock::now();

    // Process folders
    for (const auto& folderPath : folders) {
        std::string path = folderPath;
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
                        monthlyGroups[GetYearMonth(entry.last_write_time())].push_back(entry.path());
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
                        if (file == k) { keep = true; break; }
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

    // Send email
    std::string mailServer = config.mailServer();
    auto recipients = config.recipients();

    if (!mailServer.empty() && !recipients.empty() && !config.senderEmail().empty()) {
        logFile << "[INFO] Sending email report..." << std::endl;

        std::string templatePath = config.templatePath();
        std::string templateContent;
        if (!LoadHtmlTemplate(templatePath, templateContent)) {
            logFile << "[ERROR] Could not load template: " << templatePath << std::endl;
        } else {
            ReplaceAll(templateContent, "{TIMESTAMP}", GetFormattedTimestamp());
            ReplaceAll(templateContent, "{MODE}", debugMode ? "DEBUG" : "NORMAL");
            ReplaceAll(templateContent, "{TOTAL_DELETED}", std::to_string(deleteRecords.size()));
            ReplaceAll(templateContent, "{TABLE_ROWS}", GenerateTableRows(deleteRecords));
            ReplaceAll(templateContent, "{DURATION}", FormatDuration(duration));
            ReplaceAll(templateContent, "{DRIVE_SPACE_ROWS}", GenerateDriveSpaceRows(freeSpaceBefore, freeSpaceAfter));

            std::string subject = "ArchiveCleaner Report - " + GetCurrentTimestamp();

            Mailer mailer;
            bool mailOk = true;

            if (!mailer.connect(mailServer, config.mailPort())) {
                logFile << "[ERROR] Failed to connect to mail server: " << mailServer << ":" << config.mailPort() << std::endl;
                logFile << "[ERROR] Server response: " << mailer.lastResponse() << std::endl;
                mailOk = false;
            } else {
                if (!mailer.sendMail(
                    config.senderName(), config.senderEmail(), recipients,
                    subject, templateContent,
                    config.mailAuth(), config.mailUsername(), config.mailPassword()
                )) {
                    logFile << "[ERROR] Failed to send email" << std::endl;
                    logFile << "[ERROR] Server response: " << mailer.lastResponse() << std::endl;
                    mailOk = false;
                }
                mailer.disconnect();
            }

            if (mailOk)
                logFile << "[SUCCESS] Email report sent to " << recipients.size() << " recipient(s)" << std::endl;
        }
    } else {
        logFile << "[INFO] Mail not configured, skipping email report" << std::endl;
    }

    logFile << "[INFO] Script finished." << std::endl;
    logFile.close();
    if (debugFile.is_open()) debugFile.close();

    return 0;
}
