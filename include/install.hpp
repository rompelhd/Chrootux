#ifndef INSTALL_HPP
#define INSTALL_HPP

#include <string>
#include <vector>
#include <utility>

#include "basic.hpp"

std::pair<std::string, std::string> install();

bool extractArchive(const std::string& filename, const std::string& outputDirectory);
bool downloadFile(const std::string& url, const std::string& filename);

void AutoCommands();

void mountChrootAndExecute(const std::string& rootfs_dir, const std::vector<MountData>& mount_list, const std::vector<std::string>& commands, const std::string& shell_path);

std::vector<std::pair<std::string, std::string>> getMatchingFileInfos(const char* binaryPath);

#endif
