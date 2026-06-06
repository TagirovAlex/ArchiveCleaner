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

namespace fs = std::filesystem;

// --- НАСТРОЙКИ ---
// (Настройки теперь управляются через аргументы командной строки)
// -----------------

// Функция для получения текущей даты и времени в формате строки (для имен файлов)
std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm parts;
#ifdef _MSC_VER
    localtime_s(&parts, &now_c); // Безопасная версия для MSVC
#else
    parts = *std::localtime(&now_c);
#endif
    std::stringstream ss;
    ss << std::put_time(&parts, "%Y%m%d_%H%M%S");
    return ss.str();
}

// Функция для получения "ГГГГ-ММ" из времени изменения файла
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

int main(int argc, char* argv[]) {
    // Определение режима отладки через аргументы командной строки
    bool debugMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debugMode = true;
            break;
        }
    }

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
        logFile << "[INFO] DEBUG MODE ENABLED (via command line argument)" << std::endl;
        logFile << "[INFO] Deletions will be logged to " << debugLogName << std::endl;
    } else {
        logFile << "[INFO] NORMAL MODE: Files will be deleted." << std::endl;
    }

    logFile << "[INFO] Script started at " << GetCurrentTimestamp() << std::endl;

    std::ifstream configFile("folders.txt");
    if (!configFile.is_open()) {
        logFile << "[ERROR] Could not open folders.txt" << std::endl;
        return 1;
    }

    std::string path;
    while (std::getline(configFile, path)) {
        // Удаление лишних символов переноса строки (\r) для Windows файлов
        path.erase(path.find_last_not_of(" \n\r\t") + 1);
        if (path.empty()) continue;
        
        try {
            logFile << "[INFO] Processing folder: " << path << std::endl;
            
            std::map<std::string, std::vector<fs::path>> monthlyGroups;

            // Сканируем папку на наличие файлов
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

                // Сортировка файлов в месяце от самых старых к самым новым
                std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
                    return fs::last_write_time(a) < fs::last_write_time(b);
                });

                std::vector<fs::path> toKeep;
                if (files.size() == 1) {
                    toKeep.push_back(files[0]);
                } else {
                    // Оставляем самый первый файл месяца и самый последний
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
                        std::string msg = "Delete: " + file.string();
                        if (debugMode) {
                            debugFile << msg << std::endl;
                            logFile << "[DEBUG] Marked for deletion: " << file.filename().string() << std::endl;
                        } else {
                            try {
                                fs::remove(file);
                                logFile << "[SUCCESS] Deleted: " << file.filename().string() << std::endl;
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

    logFile << "[INFO] Script finished." << std::endl;
    logFile.close();
    if (debugFile.is_open()) debugFile.close();

    return 0;
}
