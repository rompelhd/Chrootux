#ifndef BASIC_HPP
#define BASIC_HPP

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

extern bool debug;

extern std::string machines_folder;

void printDebugMessage(const std::string& message);

std::string createMachinesFolderIfNotExists(const std::string& home_dir);

#endif /* BASIC_HPP */
