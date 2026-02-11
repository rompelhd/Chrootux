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
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <set>
#include <cstring>

#include <curl/curl.h>

#include <chrono>

#include <sys/statvfs.h>

#include <sys/mount.h>
#include <unordered_set>
#include <signal.h>
#include <dirent.h>

#include <ftxui/screen/screen.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace fs = std::filesystem;

bool downloadFile(const std::string& url, const std::string& filename) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error initializing libcurl." << std::endl;
        return false;
    }

    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Error opening the output file." << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    ProgressData progressData = {0.0, 0.0, 0.0};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Error downloading file: " << curl_easy_strerror(res) << std::endl;
        fclose(fp);
        curl_easy_cleanup(curl);
        return false;
    }

    fclose(fp);
    curl_easy_cleanup(curl);

    fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "Error opening the file for verification." << std::endl;
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fclose(fp);

    if (fileSize == 0) {
        std::cerr << "The downloaded file is empty." << std::endl;
        return false;
    }

    std::cout << "Download completed." << std::endl;
    return true;
}

int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    ProgressData* progressData = static_cast<ProgressData*>(clientp);
    if (!progressData) return 0;

    double percentage = (dltotal > 0) ? (static_cast<double>(dlnow) / static_cast<double>(dltotal)) * 100.0 : 0.0;

    auto currentTime = std::chrono::steady_clock::now();
    double deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - std::chrono::time_point<std::chrono::steady_clock>(std::chrono::milliseconds(static_cast<long long>(progressData->lastTime)))).count() / 1000.0;
    double downloadSpeed = (deltaTime > 0) ? (static_cast<double>(dlnow) - progressData->downloaded) / deltaTime / (1024.0 * 1024.0) : 0.0;

    progressData->downloaded = static_cast<double>(dlnow);
    progressData->lastTime = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count());

    std::cout << "Downloading " << percentage << "% | MB/s: " << downloadSpeed << "\r";
    return 0;
}

bool createDirectory(const std::string &path) {
    struct stat info;

    if (stat(path.c_str(), &info) != 0) {
        if (mkdir(path.c_str(), 0700) != 0) {
            std::cerr << "Error al crear el directorio: " << path << std::endl;
            return false;
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        std::cerr << path << " ya existe pero no es un directorio" << std::endl;
        return false;
    }

    return true;
}


int setupChrootuxConfig() {
    std::string homeDir;
    if (isTermux()) {
        homeDir = "/data/data/com.termux/files/home";
    } else {
        const char* envHome = getenv("HOME");
        if (!envHome) {
            std::cerr << "Could not determine the HOME directory." << std::endl;
            return -3;
        }
        homeDir = envHome;
    }

    std::string configDir = homeDir + "/.config/Chrootux";
    std::string configFile = configDir + "/local.conf";

    if (!fs::exists(configDir)) {
        std::cout << "Chrootux directory does not exist. Creating..." << std::endl;
        if (createDirectory(configDir)) {
            std::cout << "Chrootux directory created successfully." << std::endl;
        } else {
            std::cerr << "Error creating the Chrootux directory." << std::endl;
            return -1;
        }
    }

    if (!fs::exists(configFile)) {
        std::cout << "local.conf file does not exist. Creating..." << std::endl;
        std::ofstream outFile(configFile);
        if (outFile.is_open()) {
            outFile << "# Chrootux Config\n\n";
            outFile << "machines_folder=\"" << homeDir << "/machines\"\n";
            outFile.close();
            std::cout << "local.conf file created successfully." << std::endl;
        } else {
            std::cerr << "Error creating the local.conf file." << std::endl;
            return -2;
        }
    }

    return 1;
}

int getTerminalWidth() {
    winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

unsigned long long getFilesystemID(const std::string& path) {
    struct statvfs fsInfo;
    if (statvfs(path.c_str(), &fsInfo) != 0) {
        perror("statvfs failed");
        return 0;
    }
    return fsInfo.f_fsid;
}

bool checkTargetsMounted(const std::string& base_path, const std::string& dir) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    std::unordered_set<std::string> targets_found;

    std::vector<std::string> targets = {"/proc", "/dev", "/sys"};

    while (std::getline(mounts, line)) {
        if (line.find(base_path + "/" + dir) != std::string::npos) {
            for (const auto& target : targets) {
                if (line.find(target) != std::string::npos) {
                    targets_found.insert(target);
                }
            }
        }
    }

    return targets_found.size() == 3;
}

bool isMounted(const std::string& path) {
    struct statvfs fsInfo;
    return statvfs(path.c_str(), &fsInfo) == 0;
}

std::string extractDirectoryBefore(const std::string& path, const std::vector<std::string>& targets) {
    for (const auto& target : targets) {
        size_t pos = path.find(target);
        if (pos != std::string::npos) {
            size_t lastSlash = path.rfind('/', pos - 1);
            if (lastSlash != std::string::npos) {
                return path.substr(lastSlash + 1, pos - lastSlash - 1);
            }
        }
    }
    return "";
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
        machines_folder = home_dir + "/machines";
    }

    struct stat info;
    if (stat(machines_folder.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
        printDebugMessage("The folder 'machines' already exists in: " + machines_folder);
        return machines_folder;
    }

    if (!machines_folder.empty()) {
        if (mkdir(machines_folder.c_str(), 0755) == 0) {
            std::cout << "The following folder has been created 'machines' in: " << machines_folder << std::endl;
            return machines_folder;
        } else {
            std::cerr << "Error creating folder 'machines' in: " << machines_folder << std::endl;
            return "";
        }
    } else {
        std::cerr << "The path to the folder could not be obtained 'machines'." << std::endl;
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
        std::cerr << "Error reading directory: " << e.what() << std::endl;
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
    const char* sizes[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    double bytes = size;
    while (bytes >= 1024 && i < static_cast<int>(sizeof(sizes) / sizeof(sizes[0])) - 1) {
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
    archost = "aarch64";
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


struct ElfHeader {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
};

bool readElfHeader(const std::string& filename, ElfHeader& header) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }

    file.read(reinterpret_cast<char*>(&header.ident), sizeof(header.ident));

    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F') {
        std::cerr << "Error: Not a valid ELF file: " << filename << std::endl;
        return false;
    }

    file.seekg(0x10);
    file.read(reinterpret_cast<char*>(&header.type), sizeof(header.type));
    file.read(reinterpret_cast<char*>(&header.machine), sizeof(header.machine));

    file.close();
    return true;
}

std::string archoutinfo(const std::string& bin_path) {
    std::vector<std::string> binaries = {"bash", "busybox", "apt", "sh", "ls"};
    std::string arch = "unknown";

    for (const auto& binary : binaries) {
        std::string full_path = bin_path + "/" + binary;

        ElfHeader header;
        if (readElfHeader(full_path, header)) {
            switch (header.machine) {
                case 0x28:
                    arch = "arm";
                    break;
                case 0x3:
                    arch = "x86";
                    break;
                case 0x3E:
                    arch = "x86_64";
                    break;
                case 0xB7:
                    arch = "arm64";
                    break;
                default:
                    arch = "unknown";
                    break;
            }
            break;
        }
    }

    return arch;
}

const char* check_emulation(const char* arch, const char* archost) {
    if ((strcmp(arch, "arm") == 0 && strcmp(archost, "arm64") == 0) ||
        (strcmp(arch, "x86") == 0 && strcmp(archost, "x86_64") == 0)) {
        return "Native";
    } else if ((strcmp(arch, "arm64") == 0 && strcmp(archost, "arm") == 0) ||
               (strcmp(arch, "x86_64") == 0 && strcmp(archost, "x86") == 0) ||
               (strcmp(arch, "arm64") == 0 && strcmp(archost, "x86") == 0) ||
               (strcmp(arch, "x86_64") == 0 && strcmp(archost, "arm") == 0) ||
               (strcmp(arch, "arm") == 0 && strcmp(archost, "x86_64") == 0) ||
               (strcmp(arch, "x86") == 0 && strcmp(archost, "arm64") == 0)) {
        return "Needed";
    } else if (strcmp(arch, archost) == 0) {
        return "No";
    } else {
        return "Unknown";
    }
}

using namespace ftxui;

void machinesOn() {
    std::vector<std::string> directories = getDirectories(machines_folder);
    int terminalWidth = getTerminalWidth();
    const int card_width = 30;

    std::vector<Element> rows;
    std::vector<Element> current_row;
    int current_width = 0;

    for (const std::string& dir : directories) {
        std::filesystem::path fullPath(dir);
        std::string truncatedPath = fullPath.lexically_relative("/data/data/com.termux/files/").string();

        std::filesystem::path etc_path = fullPath / "etc";
        std::filesystem::path bin_path = fullPath / "bin";

        std::string os_name = "N/A";
        std::string size = "N/A";
        std::string arch = "N/A";

        if (exists(etc_path)) {
            auto os_release_path = etc_path / "os-release";
            if (exists(os_release_path)) {
                try {
                    os_name = getNameField(os_release_path.string());
                    if (os_name.empty()) os_name = "NAME not found";
                } catch (...) {
                    os_name = "Error reading file";
                }
            } else {
                os_name = "os-release missing";
            }
        } else {
            os_name = "etc missing";
        }

        size = formatSize(getDirectorySize(dir));
        arch = archoutinfo(bin_path);
        std::string archost = archchecker();
        const char* emulation = check_emulation(arch.c_str(), archost.c_str());

        Element card = vbox({
            text("OS: " + os_name) | bold,
            text("Size: " + size),
            text("Path: " + truncatedPath),
            text("Arch: " + arch),
            text("Emulation: " + std::string(emulation))
        }) | border | color(Color::Blue);

        if (current_width + card_width > terminalWidth) {
            rows.push_back(hbox(current_row));
            current_row.clear();
            current_width = 0;
        }

        current_row.push_back(card);
        current_width += card_width;
    }

    if (!current_row.empty()) {
        rows.push_back(hbox(current_row));
    }

    Element document = vbox(rows);

    auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
    Render(screen, document);
    screen.Print();
}

void checkMounts(const std::string& base_path) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    std::vector<std::string> targets = {"/proc", "/dev", "/sys"};
    std::unordered_set<std::string> checked_dirs;

    while (std::getline(mounts, line)) {
        if (line.find(base_path) != std::string::npos) {
            std::string mount_point = line.substr(line.find(base_path));
            std::string dir = extractDirectoryBefore(mount_point, targets);

            if (!dir.empty() && checked_dirs.find(dir) == checked_dirs.end()) {
                if (checkTargetsMounted(base_path, dir)) {
                    std::cout << "It was already mounted /proc /dev /sys towards: " << dir << "\n" << std::endl;
                }
                checked_dirs.insert(dir);
            }
        }
    }
}

bool isMtedAM(const std::string& machines_folder) {
    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(mounts, line)) {
        if (line.find(machines_folder) != std::string::npos) {
            return true;
        }
    }

    return false;
}

void checkMachinesFolder() {
    const char* home_dir = std::getenv("HOME");
        if (home_dir != nullptr) {
            std::string machines_folder = createMachinesFolderIfNotExists(home_dir);
            if (machines_folder.empty()) {
                std::cerr << "Error: creating or getting the 'machines' folder." << std::endl;
            }

            unsigned long long rootFsid = getFilesystemID(machines_folder);
            if (rootFsid != 0) {
                checkMounts(machines_folder);
            } else {
                std::cerr << "It was not possible to obtain information on the assemblies for " << machines_folder << std::endl;
            }
        } else {
            std::cerr << "The location of the home folder could not be obtained." << std::endl;
        }
    }

void killChrootProcesses(const std::string& chroot_path) {
    DIR* dir = opendir("/proc");
    if (!dir) {
        std::cerr << "Error opening /proc: " << strerror(errno) << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR || !isdigit(entry->d_name[0])) {
            continue;
        }

        std::string pid = entry->d_name;
        std::string cmdline_path = "/proc/" + pid + "/cmdline";

        std::ifstream cmdline_file(cmdline_path);
        if (cmdline_file) {
            std::string cmdline;
            std::getline(cmdline_file, cmdline);

            if (cmdline.find(chroot_path) != std::string::npos) {
                std::cout << "Process found: " << pid << " - " << cmdline << std::endl;

                if (kill(std::stoi(pid), SIGKILL) == 0) {
                    std::cout << "Process " << pid << " killed correctly.." << std::endl;
                } else {
                    std::cerr << "Error killing the process " << pid << ": " << strerror(errno) << std::endl;
                }
            }
        }
    }

    closedir(dir);
}

void killChrootuxProcesses() {
    DIR* dir = opendir("/proc");
    struct dirent* entry;

    if (dir == nullptr) {
        std::cerr << "Error opening /proc: " << strerror(errno) << std::endl;
        return;
    }

    std::vector<pid_t> pids_to_kill;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            std::string pid_str(entry->d_name);
            std::string cmdline_file = "/proc/" + pid_str + "/cmdline";
            FILE* cmd_file = fopen(cmdline_file.c_str(), "r");

            if (cmd_file) {
                char cmdline[1024];
                if (fgets(cmdline, sizeof(cmdline), cmd_file)) {
                    if (std::string(cmdline).find("Chrootux") != std::string::npos) {
                        pid_t pid = std::stoi(pid_str);
                        pids_to_kill.push_back(pid);
                    }
                }
                fclose(cmd_file);
            }
        }
    }

    closedir(dir);

    for (pid_t pid : pids_to_kill) {
        std::cout << "Killing process with PID: " << pid << std::endl;
        if (kill(pid, SIGKILL) == 0) {
            std::cout << "Process " << pid << " killed." << std::endl;
        } else {
            std::cerr << "Error killing the process " << pid << ": " << strerror(errno) << std::endl;
        }
    }
}

void UnmountAll(const std::string& machines_folder) {
    try {
        killChrootProcesses(machines_folder);
        bool stillMounted = isMtedAM(machines_folder);

        while (stillMounted) {
            for (const auto& entry : fs::directory_iterator(machines_folder)) {
                if (entry.is_directory()) {
                    std::string path = entry.path().string();
                    std::cout << "Forcing disassembly of: " << path << std::endl;

                    int result = umount2(path.c_str(), MNT_DETACH);
                    if (result != 0) {
                        std::cerr << "Error forcing disassembly of " << path
                                  << ": " << strerror(errno) << std::endl;
                    } else {
                        std::cout << "Successful disassembly of: " << path << std::endl;
                    }

                    for (const auto& subentry : fs::directory_iterator(path)) {
                        if (subentry.is_directory()) {
                            std::string subpath = subentry.path().string();
                            std::cout << "Disassembling subdirectory: " << subpath << std::endl;
                            umount2(subpath.c_str(), MNT_DETACH);
                        }
                    }
                }
            }

            stillMounted = isMtedAM(machines_folder);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    killChrootuxProcesses();
}
