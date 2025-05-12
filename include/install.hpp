#ifndef INSTALL_HPP
#define INSTALL_HPP

#include <string>
#include <vector>
#include <utility>

#include "basic.hpp"

struct OperatingSystem {
        std::string name;
        std::string url;
};

struct InstallResult {
    std::string name;
    std::string url;
    std::string filename;
};

InstallResult install(const std::string& archost);

int showMenu(const std::vector<OperatingSystem>& osList);

std::pair<std::string, bool> extractArchive(const std::string& filename, const std::string& outputDirectory);

void AutoCommands(const std::string& name, const std::string& outputDir);

void mountChrootAndExecute(const std::string& rootfs_dir, const std::vector<MountData>& mount_list, const std::vector<std::string>& commands, const std::string& shell_path);

std::vector<std::pair<std::string, std::string>> getMatchingFileInfos(const char* binaryPath);

#endif
