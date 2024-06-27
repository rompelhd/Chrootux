#ifndef INSTALL_HPP
#define INSTALL_HPP

#include <string>
#include <vector>
#include <utility>

std::pair<std::string, std::string> install();

bool extractArchive(const std::string& filename, const std::string& outputDirectory);
bool downloadFile(const std::string& url, const std::string& filename);

extern std::string arch;

std::vector<std::pair<std::string, std::string>> getMatchingFileInfos(const char* binaryPath);
std::string archchecker();

#endif
