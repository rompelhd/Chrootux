#ifndef BASIC_HPP
#define BASIC_HPP

#include <vector>
#include <string>

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

void hightable(int totalWidth);

size_t getRealLength(const std::string& str);

void intertable(int totalWidth, const std::string& li_content, int terminalWidth);

void lowtable(int totalWidth);


extern std::string os_name;

extern bool debug;

extern std::string machines_folder;

void printDebugMessage(const std::string& message);

std::vector<std::string> getDirectories(const std::string& folderPath);

void machinesOn();

void getInstalledPackages();

std::string createMachinesFolderIfNotExists(const std::string& home_dir);

#endif /* BASIC_HPP */
