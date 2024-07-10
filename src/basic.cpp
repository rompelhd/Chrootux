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
#include <sys/ioctl.h>
#include <set>

int getTerminalWidth() {
    winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

void hightable(int totalWidth) {
    std::string details = "Arch";
    std::string action = "";
    std::cout << Colours::blueColour << "╭─" << action << "[ " << Colours::yellowColour << details << Colours::blueColour << " ] ";
    for (int i = 26; i < totalWidth - 1; ++i) {
        std::cout << "─";
    }
    std::cout << "╮" << std::endl;
    std::cout << Colours::blueColour << "│";                                                                                        for (int i = 1; i < totalWidth - 1; ++i) {
        std::cout << " ";
    }
    std::cout << Colours::blueColour << "│" << std::endl;
}

size_t getRealLength(const std::string& str) {
    size_t length = 0;
    bool inEscape = false;
                                                                                                                                    for (char c : str) {
        if (c == '\033') {
            inEscape = true;
        } else if (inEscape && c == 'm') {
            inEscape = false;
        } else if (!inEscape) {
            length++;
        }
    }

    return length;
}

void intertable(int totalWidth, const std::string& li_content, int terminalWidth) {
    std::string output = "\033[34m│  [*] \033[33m" + li_content + "\033[0m";
    size_t realLength = getRealLength(output);
    size_t spacesNeeded = totalWidth - realLength;

    std::string additionalSpacesFirst;
    std::string additionalSpacesSecond;
      if (realLength >= terminalWidth) {
        size_t lastSpace = output.rfind(' ', terminalWidth);

        std::string firstLine = output.substr(0, lastSpace);                                                                            std::string secondLine = output.substr(lastSpace + 1);

        size_t firstLineLength = getRealLength(firstLine);
        size_t secondLineLength = getRealLength(secondLine);

        size_t remainingSpacesFirst = totalWidth - firstLineLength + 1;
        for (size_t i = 0; i < remainingSpacesFirst; ++i) {
            additionalSpacesFirst += " ";
        }

        size_t remainingSpacesSecond = totalWidth - secondLineLength - 8;
        for (size_t i = 0; i < remainingSpacesSecond; ++i) {
            additionalSpacesSecond += " ";
        }

        std::cout << firstLine << additionalSpacesFirst << Colours::blueColour << "│" << std::endl;
        std::cout << Colours::blueColour << "│      " << Colours::yellowColour << secondLine << additionalSpacesSecond << Colours::blueColour << "│" << std::endl;


    } else {
        std::cout << Colours::blueColour << output;
        for (int i = 0; i < spacesNeeded; ++i) {
            std::cout << " ";
        }
        std::cout << Colours::blueColour << " │";
    }
}

void lowtable(int totalWidth) {
    std::cout << Colours::blueColour << "│";
    for (int i = 1; i < totalWidth - 1; ++i) {                                                                                          std::cout << " ";
    }
    std::cout << "│" << std::endl;

    std::cout << Colours::blueColour << "╰";
    for (int i = 1; i < totalWidth - 1; ++i) {
        std::cout << "─";
    }
    std::cout << "╯" << std::endl;
}



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

std::string archost;

std::string archchecker() {
#if defined(_M_X64) || defined(__amd64__)
    archost = "x86_64";
#elif defined(_M_IX86) || defined(__i386__)
    archost = "x86";
#elif defined(_M_ARM) || defined(__arm__)
    archost = "arm";
#elif defined(_M_ARM64) || defined(__aarch64__)
    archost = "arm64";
#elif defined(__linux__)
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        archost = buffer.machine;
    } else {
        archost = "unknown";
    }
#else
    archost = "unknown";
#endif
    return archost;
}


std::string archoutinfo(const std::string& bin_path) {
    std::vector<std::string> binaries = {"bash", "busybox", "apt", "sh", "ls"};
    std::string arch = "unknown";

    for (const auto& binary : binaries) {
        std::string command = "file " + bin_path + "/" + binary;
        char buffer[128];
        std::string result;
        std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) throw std::runtime_error("popen() failed!");

        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result += buffer;
        }

        std::size_t lsb_pos = result.find("LSB");
        if (lsb_pos != std::string::npos) {
            std::size_t start = result.find(" ", lsb_pos) + 1;
            std::size_t end = result.find(",", start);
            if (start != std::string::npos && end != std::string::npos) {
                arch = result.substr(start, end - start);
            }
        }
    }

    std::vector<std::string> known_architectures = {"arm", "arm64", "x86", "x86_64"};
    if (std::find(known_architectures.begin(), known_architectures.end(), arch) == known_architectures.end()) {
        arch = "unknown";
    }

    return arch;
}

void machinesOn() {
    std::vector<std::string> directories = getDirectories(machines_folder);

    std::cout << "OS Name" << std::setw(30) << std::setw(15) << "Size" << std::setw(15) << "Path" << std::setw(30) << "Arch" << std::endl;
    std::cout << std::string(75, '-') << std::endl;

    for (const std::string& dir : directories) {
        std::filesystem::path fullPath(dir);
        std::string truncatedPath = fullPath.lexically_relative("/data/data/com.termux/files/").string();

        std::filesystem::path etc_path = std::filesystem::path(dir) / "etc";
        std::filesystem::path bin_path = std::filesystem::path(dir) / "bin";

        std::string os_name = "N/A";
        std::string size = "N/A";
        std::string arch = "N/A";

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
        arch = archoutinfo(bin_path);

        std::cout << Colours::greenColour << std::left << std::setw(18) << os_name
                  << Colours::redColour << std::setw(15) << size
                  << std::setw(30) << truncatedPath << arch << Colours::endColour << std::endl;
    }
    std::cout << "\n" << std::endl;
}
