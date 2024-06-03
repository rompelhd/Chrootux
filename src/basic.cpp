#include "basic.hpp"
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <string>
#include <regex>

std::string os_name;

bool debug = false;
std::string machines_folder;

void printDebugMessage(const std::string& message) {
    if (debug) {
        std::cerr << "[DEBUG] " << message << std::endl;
    }
}

void setTermuxPath() {
    const char* termuxPath = "/data/data/com.termux/files/usr/bin";
    const char* currentPath = std::getenv("PATH");

    if (currentPath) {
        std::string newPath = termuxPath;
        newPath += ":";
        newPath += currentPath;
        setenv("PATH", newPath.c_str(), 1);
    } else {
        setenv("PATH", termuxPath, 1);
    }
}

bool isTermux() {
    struct stat st;
    if (stat("/data/data/com.termux/files/home", &st) == 0) {
        return true;
    } else {
        return false;
    }
}

std::string createMachinesFolderIfNotExists(const std::string& home_dir) {
    if (isTermux()) {
        setTermuxPath();
        std::string home_folder = "/data/data/com.termux/files/home/";
        machines_folder = home_folder + "machines";
    } else {
        std::cout << "No estás en Termux." << std::endl;
        return "";
    }

    struct stat info;
    if (stat(machines_folder.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
        printDebugMessage("La carpeta 'machines' ya existe en: " + machines_folder);
        return machines_folder;
    }

    if (!machines_folder.empty()) {
        if (mkdir(machines_folder.c_str(), 0755) == 0) {
            std::cout << "Se ha creado la carpeta 'machines' en: " << machines_folder << std::endl;
            return machines_folder;
        } else {
            std::cerr << "Error al crear la carpeta 'machines' en: " << machines_folder << std::endl;
            return "";
        }
    } else {
        std::cerr << "No se pudo obtener la ruta de la carpeta 'machines'." << std::endl;
        return "";
    }
}

std::vector<std::string> getDirectories(const std::string& folderPath) {
    std::vector<std::string> directories;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
            if (entry.is_directory()) {
                directories.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error al leer el directorio: " << e.what() << std::endl;
    }
    return directories;
}

bool exists(const std::string& path) {
    return std::filesystem::exists(path);
}

std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

std::string getNameField(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("NAME=") == 0) {
            file.close();
            return line.substr(6, line.size() - 7);
        }
    }
    file.close();
    return "";
}

std::string formatSize(long long size) {
    const char *sizes[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    double bytes = size;
    while (bytes >= 1024 && i < (sizeof(sizes) / sizeof(sizes[0])) - 1) {
        bytes /= 1024;
        i++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << bytes << " " << sizes[i];
    return ss.str();
}

long long getDirectorySize(const std::string& path) {
    long long size = 0;
    for (const auto& file : std::filesystem::recursive_directory_iterator(path)) {
        if (std::filesystem::is_regular_file(file)) {
            size += std::filesystem::file_size(file);
        }
    }
    return size;
}

void machinesOn() {
    std::vector<std::string> directories = getDirectories(machines_folder);

    std::cout << "OS Name" << std::setw(30) << std::setw(15) << "Size" << std::setw(15) << "Path" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    for (const std::string& dir : directories) {
        std::filesystem::path fullPath(dir);
        std::string truncatedPath = fullPath.lexically_relative("/data/data/com.termux/files/").string();

        std::filesystem::path etc_path = std::filesystem::path(dir) / "etc";
        std::string os_name = "N/A";
        std::string size = "N/A";

        if (exists(etc_path)) {
            std::filesystem::path os_release_path = etc_path / "os-release";
            if (exists(os_release_path)) {
                try {
                    os_name = getNameField(os_release_path.string());
                    if (os_name.empty()) {
                        os_name = "NAME field not found";
                    }
                } catch (const std::exception& e) {
                    os_name = "Error reading file";
                }
            } else {
                os_name = "'os-release' not found";
            }
        } else {
            os_name = "'etc' not found";
        }

        size = formatSize(getDirectorySize(dir));

        std::cout << Colours::greenColour << std::left << std::setw(18) << os_name
                  << Colours::redColour << std::setw(15) << size
                  << std::setw(30) << truncatedPath << Colours::endColour << std::endl;
    }
    std::cout << "\n" << std::endl;
}
