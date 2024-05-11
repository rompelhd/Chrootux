#ifndef INSTALL_HPP
#define INSTALL_HPP

#include <string>
#include <vector>
#include <utility>

void install();

bool extractArchive(const std::string& filename, const std::string& outputDirectory);
bool downloadFile(const std::string& url, const std::string& filename);

std::vector<std::pair<std::string, std::string>> getMatchingFileInfos(const char* binaryPath);
int archchecker();

#endif
