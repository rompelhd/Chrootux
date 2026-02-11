#ifndef BASIC_HPP
#define BASIC_HPP

#include <vector>
#include <string>
#include <chrono>
#include <curl/curl.h>

namespace Colours {
    const std::string greenColour = "\033[1;32m";
    const std::string endColour = "\033[0m";
    const std::string redColour = "\033[1;31m";
    const std::string blueColour = "\033[1;34m";
    const std::string yellowColour = "\033[1;33m";
    const std::string purpleColour = "\033[1;35m";
    const std::string turquoiseColour = "\033[1;36m";
    const std::string grayColour = "\033[1;37m";
}

struct MountData {
    std::string source;
    std::string target;
    std::string fs_type;
};

int getTerminalWidth();

bool isTermux();

extern std::string os_name;

extern bool debug;

extern std::string machines_folder;

void printDebugMessage(const std::string& message);

std::vector<std::string> getDirectories(const std::string& folderPath);

void machinesOn();

void getInstalledPackages();

std::string createMachinesFolderIfNotExists(const std::string& home_dir);

extern std::string archost;

std::string archchecker();


std::string archoutinfo(const std::string& bin_path);
const char* check_emulation(const char* arch, const char* archost);


std::string extractDirectoryBefore(const std::string& path, const std::vector<std::string>& targets);

bool checkTargetsMounted(const std::string& base_path, const std::string& dir);

unsigned long long getFilesystemID(const std::string& path);

void checkMounts(const std::string& base_path);

void checkMachinesFolder();

bool isMtedAM(const std::string& machines_folder);

void UnmountAll(const std::string& machines_folder);

bool isMounted(const std::string& path);


int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);

int setupChrootuxConfig();

bool createDirectory(const std::string &path);

bool downloadFile(const std::string &url, const std::string &outputFile);

struct ProgressData {
    double lastTime;
    double totalSize;
    double downloaded;
};

#endif /* BASIC_HPP */
